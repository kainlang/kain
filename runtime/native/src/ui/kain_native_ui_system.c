#include "kain_runtime_base.h"
#include "kain_native_ui_system.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

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
    char text[KAIN_NATIVE_UI_MAX_TEXT];
    char style_key[KAIN_NATIVE_UI_MAX_KEY];
} KainNativeUiDrawCommand;

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
    double last_delta_ms;
    char app_name[KAIN_NATIVE_UI_MAX_KEY];
    char window_title[KAIN_NATIVE_UI_MAX_TEXT];
    KainNativeUiNode nodes[KAIN_NATIVE_UI_MAX_NODES];
    KainNativeUiStyleRecord styles[KAIN_NATIVE_UI_MAX_STYLES];
    KainNativeUiDrawCommand draw_commands[KAIN_NATIVE_UI_MAX_DRAW_COMMANDS];
    KainNativeUiEvent events[KAIN_NATIVE_UI_MAX_EVENTS];
    KainNativeUiEvent active_event;
    int64_t node_count;
    int64_t style_count;
    int64_t draw_command_count;
    int64_t event_head;
    int64_t event_tail;
    int64_t event_count;
} KainNativeUiSession;

static KainNativeUiSession g_sessions[KAIN_NATIVE_UI_MAX_SESSIONS];
static int64_t g_next_session_id = 1;
static char g_empty_string[] = "";

static void kain_native_ui_copy_text(char* destination, size_t destination_size, const char* source) {
    if (!destination || destination_size == 0) {
        return;
    }
    if (!source) {
        source = "";
    }
    snprintf(destination, destination_size, "%s", source);
}

static const char* kain_native_ui_return_string(const char* source) {
    return string_new((char*)(source ? source : g_empty_string));
}

static KainNativeUiSession* kain_native_ui_find_session(int64_t session_id) {
    int64_t index;
    if (session_id <= 0) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_SESSIONS; index += 1) {
        if (g_sessions[index].in_use && g_sessions[index].id == session_id) {
            return &g_sessions[index];
        }
    }
    return NULL;
}

static KainNativeUiNode* kain_native_ui_find_node(KainNativeUiSession* session, int64_t node_id) {
    int64_t index;
    if (!session || node_id <= 0) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_NODES; index += 1) {
        if (session->nodes[index].in_use && session->nodes[index].id == node_id) {
            return &session->nodes[index];
        }
    }
    return NULL;
}

static int64_t kain_native_ui_flag_bit(const char* flag) {
    if (!flag) {
        return 0;
    }
    if (strcmp(flag, "hidden") == 0) {
        return KAIN_NATIVE_UI_NODE_HIDDEN;
    }
    if (strcmp(flag, "visible") == 0) {
        return KAIN_NATIVE_UI_NODE_HIDDEN;
    }
    if (strcmp(flag, "focusable") == 0) {
        return KAIN_NATIVE_UI_NODE_FOCUSABLE;
    }
    if (strcmp(flag, "interactive") == 0) {
        return KAIN_NATIVE_UI_NODE_INTERACTIVE;
    }
    if (strcmp(flag, "disabled") == 0) {
        return KAIN_NATIVE_UI_NODE_DISABLED;
    }
    return 0;
}

static int kain_native_ui_node_is_visible(const KainNativeUiNode* node) {
    return node && ((node->flags & KAIN_NATIVE_UI_NODE_HIDDEN) == 0);
}

static void kain_native_ui_touch_node(KainNativeUiSession* session, KainNativeUiNode* node, int64_t reason) {
    if (!session || !node) {
        return;
    }
    node->revision += 1;
    node->dirty_reason = reason;
    session->dirty_count += 1;
}

static KainNativeUiStyleRecord* kain_native_ui_find_style(KainNativeUiSession* session, int64_t node_id, const char* key) {
    int64_t index;
    if (!session || !key) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_STYLES; index += 1) {
        if (session->styles[index].in_use &&
            session->styles[index].node_id == node_id &&
            strcmp(session->styles[index].key, key) == 0) {
            return &session->styles[index];
        }
    }
    return NULL;
}

static KainNativeUiStyleRecord* kain_native_ui_ensure_style(KainNativeUiSession* session, int64_t node_id, const char* key) {
    int64_t index;
    KainNativeUiStyleRecord* existing = kain_native_ui_find_style(session, node_id, key);
    if (existing) {
        return existing;
    }
    if (!session || !key || session->style_count >= KAIN_NATIVE_UI_MAX_STYLES) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_STYLES; index += 1) {
        if (!session->styles[index].in_use) {
            memset(&session->styles[index], 0, sizeof(session->styles[index]));
            session->styles[index].in_use = 1;
            session->styles[index].node_id = node_id;
            kain_native_ui_copy_text(session->styles[index].key, sizeof(session->styles[index].key), key);
            session->style_count += 1;
            return &session->styles[index];
        }
    }
    return NULL;
}

static KainNativeUiDrawCommand* kain_native_ui_append_draw_command(KainNativeUiSession* session, const char* kind) {
    KainNativeUiDrawCommand* command;
    if (!session || session->draw_command_count >= KAIN_NATIVE_UI_MAX_DRAW_COMMANDS) {
        return NULL;
    }
    command = &session->draw_commands[session->draw_command_count];
    memset(command, 0, sizeof(*command));
    kain_native_ui_copy_text(command->kind, sizeof(command->kind), kind);
    session->draw_command_count += 1;
    return command;
}

int64_t kain_native_ui_reset(void) {
    memset(g_sessions, 0, sizeof(g_sessions));
    g_next_session_id = 1;
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_session_create(const char* app_name, int64_t width, int64_t height) {
    int64_t index;
    for (index = 0; index < KAIN_NATIVE_UI_MAX_SESSIONS; index += 1) {
        if (!g_sessions[index].in_use) {
            memset(&g_sessions[index], 0, sizeof(g_sessions[index]));
            g_sessions[index].in_use = 1;
            g_sessions[index].id = g_next_session_id++;
            g_sessions[index].width = width;
            g_sessions[index].height = height;
            g_sessions[index].next_node_id = 1;
            kain_native_ui_copy_text(g_sessions[index].app_name, sizeof(g_sessions[index].app_name), app_name);
            return g_sessions[index].id;
        }
    }
    return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
}

int64_t kain_native_ui_session_destroy(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    memset(session, 0, sizeof(*session));
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_session_count(void) {
    int64_t index;
    int64_t count = 0;
    for (index = 0; index < KAIN_NATIVE_UI_MAX_SESSIONS; index += 1) {
        if (g_sessions[index].in_use) {
            count += 1;
        }
    }
    return count;
}

int64_t kain_native_ui_window_open(int64_t session_id, const char* title, int64_t width, int64_t height) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    session->open = 1;
    session->width = width;
    session->height = height;
    kain_native_ui_copy_text(session->window_title, sizeof(session->window_title), title);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_window_close(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    session->open = 0;
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_begin_frame(int64_t session_id, double delta_ms) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    session->frame_index += 1;
    session->last_delta_ms = delta_ms;
    session->draw_command_count = 0;
    return session->frame_index;
}

int64_t kain_native_ui_end_frame(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    return session->draw_command_count;
}

int64_t kain_native_ui_present(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    session->last_presented_frame = session->frame_index;
    session->dirty_count = 0;
    return session->last_presented_frame;
}

int64_t kain_native_ui_frame_index(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->frame_index : KAIN_NATIVE_UI_INVALID_SESSION;
}

int64_t kain_native_ui_last_presented_frame(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->last_presented_frame : KAIN_NATIVE_UI_INVALID_SESSION;
}

int64_t kain_native_ui_node_create(int64_t session_id, const char* kind) {
    int64_t index;
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (session->node_count >= KAIN_NATIVE_UI_MAX_NODES) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_NODES; index += 1) {
        if (!session->nodes[index].in_use) {
            memset(&session->nodes[index], 0, sizeof(session->nodes[index]));
            session->nodes[index].in_use = 1;
            session->nodes[index].id = session->next_node_id++;
            session->nodes[index].flags = KAIN_NATIVE_UI_NODE_FOCUSABLE | KAIN_NATIVE_UI_NODE_INTERACTIVE;
            kain_native_ui_copy_text(session->nodes[index].kind, sizeof(session->nodes[index].kind), kind);
            session->node_count += 1;
            kain_native_ui_touch_node(session, &session->nodes[index], 1);
            return session->nodes[index].id;
        }
    }
    return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
}

int64_t kain_native_ui_node_destroy(int64_t session_id, int64_t node_id) {
    int64_t index;
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_NODES; index += 1) {
        if (session->nodes[index].in_use && session->nodes[index].parent_id == node_id) {
            session->nodes[index].parent_id = 0;
        }
    }
    if (session->focused_node_id == node_id) {
        session->focused_node_id = 0;
    }
    memset(node, 0, sizeof(*node));
    session->node_count -= 1;
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_node_count(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->node_count : KAIN_NATIVE_UI_INVALID_SESSION;
}

int64_t kain_native_ui_node_exists(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return kain_native_ui_find_node(session, node_id) ? 1 : 0;
}

int64_t kain_native_ui_node_set_parent(int64_t session_id, int64_t node_id, int64_t parent_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    KainNativeUiNode* old_parent;
    KainNativeUiNode* new_parent;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    if (parent_id == node_id) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    if (parent_id > 0 && !kain_native_ui_find_node(session, parent_id)) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    old_parent = kain_native_ui_find_node(session, node->parent_id);
    new_parent = kain_native_ui_find_node(session, parent_id);
    if (old_parent && old_parent->child_count > 0) {
        old_parent->child_count -= 1;
    }
    if (new_parent) {
        new_parent->child_count += 1;
    }
    node->parent_id = parent_id;
    kain_native_ui_touch_node(session, node, 2);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_node_parent(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    return node ? node->parent_id : KAIN_NATIVE_UI_INVALID_NODE;
}

int64_t kain_native_ui_node_child_count(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    return node ? node->child_count : KAIN_NATIVE_UI_INVALID_NODE;
}

int64_t kain_native_ui_node_set_rect(
    int64_t session_id,
    int64_t node_id,
    double x,
    double y,
    double width,
    double height
) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    node->x = x;
    node->y = y;
    node->width = width;
    node->height = height;
    kain_native_ui_touch_node(session, node, 3);
    return KAIN_NATIVE_UI_OK;
}

double kain_native_ui_node_x(int64_t session_id, int64_t node_id) {
    KainNativeUiNode* node = kain_native_ui_find_node(kain_native_ui_find_session(session_id), node_id);
    return node ? node->x : 0.0;
}

double kain_native_ui_node_y(int64_t session_id, int64_t node_id) {
    KainNativeUiNode* node = kain_native_ui_find_node(kain_native_ui_find_session(session_id), node_id);
    return node ? node->y : 0.0;
}

double kain_native_ui_node_width(int64_t session_id, int64_t node_id) {
    KainNativeUiNode* node = kain_native_ui_find_node(kain_native_ui_find_session(session_id), node_id);
    return node ? node->width : 0.0;
}

double kain_native_ui_node_height(int64_t session_id, int64_t node_id) {
    KainNativeUiNode* node = kain_native_ui_find_node(kain_native_ui_find_session(session_id), node_id);
    return node ? node->height : 0.0;
}

int64_t kain_native_ui_node_set_text(int64_t session_id, int64_t node_id, const char* text) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    kain_native_ui_copy_text(node->text, sizeof(node->text), text);
    kain_native_ui_touch_node(session, node, 4);
    return KAIN_NATIVE_UI_OK;
}

const char* kain_native_ui_node_text(int64_t session_id, int64_t node_id) {
    KainNativeUiNode* node = kain_native_ui_find_node(kain_native_ui_find_session(session_id), node_id);
    return kain_native_ui_return_string(node ? node->text : g_empty_string);
}

const char* kain_native_ui_node_kind(int64_t session_id, int64_t node_id) {
    KainNativeUiNode* node = kain_native_ui_find_node(kain_native_ui_find_session(session_id), node_id);
    return kain_native_ui_return_string(node ? node->kind : g_empty_string);
}

int64_t kain_native_ui_node_set_flag(int64_t session_id, int64_t node_id, const char* flag, int64_t enabled) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    int64_t bit = kain_native_ui_flag_bit(flag);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    if (bit == 0) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    if (flag && strcmp(flag, "visible") == 0) {
        enabled = enabled ? 0 : 1;
    }
    if (enabled) {
        node->flags |= bit;
    } else {
        node->flags &= ~bit;
    }
    kain_native_ui_touch_node(session, node, 5);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_node_has_flag(int64_t session_id, int64_t node_id, const char* flag) {
    KainNativeUiNode* node = kain_native_ui_find_node(kain_native_ui_find_session(session_id), node_id);
    int64_t bit = kain_native_ui_flag_bit(flag);
    if (!node || bit == 0) {
        return 0;
    }
    if (flag && strcmp(flag, "visible") == 0) {
        return (node->flags & bit) == 0 ? 1 : 0;
    }
    return (node->flags & bit) != 0 ? 1 : 0;
}

int64_t kain_native_ui_node_set_style_i64(int64_t session_id, int64_t node_id, const char* key, int64_t value) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiStyleRecord* record;
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    record = kain_native_ui_ensure_style(session, node_id, key);
    if (!record) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    record->value_kind = KAIN_NATIVE_UI_STYLE_I64;
    record->i64_value = value;
    kain_native_ui_touch_node(session, node, 6);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_node_set_style_f64(int64_t session_id, int64_t node_id, const char* key, double value) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiStyleRecord* record;
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    record = kain_native_ui_ensure_style(session, node_id, key);
    if (!record) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    record->value_kind = KAIN_NATIVE_UI_STYLE_F64;
    record->f64_value = value;
    kain_native_ui_touch_node(session, node, 6);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_node_set_style_string(int64_t session_id, int64_t node_id, const char* key, const char* value) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiStyleRecord* record;
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    record = kain_native_ui_ensure_style(session, node_id, key);
    if (!record) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    record->value_kind = KAIN_NATIVE_UI_STYLE_STRING;
    kain_native_ui_copy_text(record->string_value, sizeof(record->string_value), value);
    kain_native_ui_touch_node(session, node, 6);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_node_style_i64(int64_t session_id, int64_t node_id, const char* key, int64_t fallback) {
    KainNativeUiStyleRecord* record = kain_native_ui_find_style(kain_native_ui_find_session(session_id), node_id, key);
    return (record && record->value_kind == KAIN_NATIVE_UI_STYLE_I64) ? record->i64_value : fallback;
}

double kain_native_ui_node_style_f64(int64_t session_id, int64_t node_id, const char* key, double fallback) {
    KainNativeUiStyleRecord* record = kain_native_ui_find_style(kain_native_ui_find_session(session_id), node_id, key);
    return (record && record->value_kind == KAIN_NATIVE_UI_STYLE_F64) ? record->f64_value : fallback;
}

const char* kain_native_ui_node_style_string(int64_t session_id, int64_t node_id, const char* key, const char* fallback) {
    KainNativeUiStyleRecord* record = kain_native_ui_find_style(kain_native_ui_find_session(session_id), node_id, key);
    if (record && record->value_kind == KAIN_NATIVE_UI_STYLE_STRING) {
        return kain_native_ui_return_string(record->string_value);
    }
    return kain_native_ui_return_string(fallback ? fallback : g_empty_string);
}

int64_t kain_native_ui_focus(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    if ((node->flags & KAIN_NATIVE_UI_NODE_DISABLED) != 0) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    session->focused_node_id = node_id;
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_focused_node(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->focused_node_id : KAIN_NATIVE_UI_INVALID_SESSION;
}

int64_t kain_native_ui_hit_test(int64_t session_id, double x, double y) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    int64_t index;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    for (index = KAIN_NATIVE_UI_MAX_NODES - 1; index >= 0; index -= 1) {
        KainNativeUiNode* node = &session->nodes[index];
        if (!node->in_use || !kain_native_ui_node_is_visible(node)) {
            continue;
        }
        if (node->width <= 0.0 || node->height <= 0.0) {
            continue;
        }
        if (x >= node->x && x <= node->x + node->width && y >= node->y && y <= node->y + node->height) {
            return node->id;
        }
    }
    return 0;
}

int64_t kain_native_ui_mark_dirty(int64_t session_id, int64_t node_id, int64_t reason) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    kain_native_ui_touch_node(session, node, reason);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_dirty_count(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->dirty_count : KAIN_NATIVE_UI_INVALID_SESSION;
}

int64_t kain_native_ui_push_event(
    int64_t session_id,
    const char* kind,
    int64_t target_node_id,
    double x,
    double y,
    int64_t key_code,
    const char* text
) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiEvent* event;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (session->event_count >= KAIN_NATIVE_UI_MAX_EVENTS) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    event = &session->events[session->event_tail];
    memset(event, 0, sizeof(*event));
    kain_native_ui_copy_text(event->kind, sizeof(event->kind), kind);
    event->target_node_id = target_node_id;
    event->x = x;
    event->y = y;
    event->key_code = key_code;
    kain_native_ui_copy_text(event->text, sizeof(event->text), text);
    session->event_tail = (session->event_tail + 1) % KAIN_NATIVE_UI_MAX_EVENTS;
    session->event_count += 1;
    return session->event_count;
}

int64_t kain_native_ui_poll_event(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (session->event_count <= 0) {
        memset(&session->active_event, 0, sizeof(session->active_event));
        return 0;
    }
    session->active_event = session->events[session->event_head];
    memset(&session->events[session->event_head], 0, sizeof(session->events[session->event_head]));
    session->event_head = (session->event_head + 1) % KAIN_NATIVE_UI_MAX_EVENTS;
    session->event_count -= 1;
    return 1;
}

const char* kain_native_ui_event_kind(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return kain_native_ui_return_string(session ? session->active_event.kind : g_empty_string);
}

int64_t kain_native_ui_event_target(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->active_event.target_node_id : KAIN_NATIVE_UI_INVALID_SESSION;
}

double kain_native_ui_event_x(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->active_event.x : 0.0;
}

double kain_native_ui_event_y(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->active_event.y : 0.0;
}

int64_t kain_native_ui_event_key_code(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->active_event.key_code : KAIN_NATIVE_UI_INVALID_SESSION;
}

const char* kain_native_ui_event_text(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return kain_native_ui_return_string(session ? session->active_event.text : g_empty_string);
}

int64_t kain_native_ui_draw_rect(
    int64_t session_id,
    int64_t node_id,
    double x,
    double y,
    double width,
    double height,
    const char* style_key
) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiDrawCommand* command;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!kain_native_ui_find_node(session, node_id)) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    command = kain_native_ui_append_draw_command(session, "rect");
    if (!command) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    command->node_id = node_id;
    command->x = x;
    command->y = y;
    command->width = width;
    command->height = height;
    kain_native_ui_copy_text(command->style_key, sizeof(command->style_key), style_key);
    return session->draw_command_count;
}

int64_t kain_native_ui_draw_text(
    int64_t session_id,
    int64_t node_id,
    double x,
    double y,
    const char* text,
    const char* style_key
) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiDrawCommand* command;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!kain_native_ui_find_node(session, node_id)) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    command = kain_native_ui_append_draw_command(session, "text");
    if (!command) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    command->node_id = node_id;
    command->x = x;
    command->y = y;
    kain_native_ui_copy_text(command->text, sizeof(command->text), text);
    kain_native_ui_copy_text(command->style_key, sizeof(command->style_key), style_key);
    return session->draw_command_count;
}

int64_t kain_native_ui_draw_command_count(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->draw_command_count : KAIN_NATIVE_UI_INVALID_SESSION;
}

const char* kain_native_ui_draw_command_kind(int64_t session_id, int64_t command_index) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session || command_index < 0 || command_index >= session->draw_command_count) {
        return kain_native_ui_return_string(g_empty_string);
    }
    return kain_native_ui_return_string(session->draw_commands[command_index].kind);
}

int64_t kain_native_ui_draw_command_node(int64_t session_id, int64_t command_index) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session || command_index < 0 || command_index >= session->draw_command_count) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    return session->draw_commands[command_index].node_id;
}
