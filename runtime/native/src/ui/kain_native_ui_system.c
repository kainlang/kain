#include "kain_native_ui_system_internal.h"
#include "kain_native_ui_host_adapter.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static KainNativeUiResource* kain_native_ui_find_resource(KainNativeUiSession* session, int64_t resource_id) {
    int64_t index;
    if (!session || resource_id <= 0) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_RESOURCES; index += 1) {
        if (session->resources[index].in_use && session->resources[index].id == resource_id) {
            return &session->resources[index];
        }
    }
    return NULL;
}

static KainNativeUiMenu* kain_native_ui_find_menu(KainNativeUiSession* session, int64_t menu_id) {
    int64_t index;
    if (!session || menu_id <= 0) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_MENUS; index += 1) {
        if (session->menus[index].in_use && session->menus[index].id == menu_id) {
            return &session->menus[index];
        }
    }
    return NULL;
}

static KainNativeUiDialog* kain_native_ui_find_dialog(KainNativeUiSession* session, int64_t dialog_id) {
    int64_t index;
    if (!session || dialog_id <= 0) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_DIALOGS; index += 1) {
        if (session->dialogs[index].in_use && session->dialogs[index].id == dialog_id) {
            return &session->dialogs[index];
        }
    }
    return NULL;
}

static KainNativeUiDrawCommand* kain_native_ui_find_draw_command(KainNativeUiSession* session, int64_t command_index) {
    if (!session || command_index < 0 || command_index >= session->draw_command_count) {
        return NULL;
    }
    return &session->draw_commands[command_index];
}

static uint64_t kain_native_ui_hash_u64(uint64_t hash, uint64_t value) {
    int shift;
    for (shift = 0; shift < 64; shift += 8) {
        hash ^= (uint8_t)((value >> shift) & 0xffu);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t kain_native_ui_hash_i64(uint64_t hash, int64_t value) {
    return kain_native_ui_hash_u64(hash, (uint64_t)value);
}

static uint64_t kain_native_ui_hash_f64(uint64_t hash, double value) {
    return kain_native_ui_hash_i64(hash, (int64_t)(value * 1000.0));
}

static uint64_t kain_native_ui_hash_text(uint64_t hash, const char* text) {
    const unsigned char* cursor = (const unsigned char*)(text ? text : g_empty_string);
    while (*cursor) {
        hash ^= *cursor;
        hash *= UINT64_C(1099511628211);
        cursor += 1;
    }
    return hash;
}

static int64_t kain_native_ui_positive_hash(uint64_t hash) {
    return (int64_t)(hash & UINT64_C(0x7fffffffffffffff));
}

typedef struct KainNativeUiToken16 {
    uint64_t length;
    uint64_t lo;
    uint64_t hi;
    uint64_t state;
} KainNativeUiToken16;

typedef struct KainNativeUiFlagInfo {
    int64_t bit;
    uint64_t visible;
} KainNativeUiFlagInfo;

static uint64_t kain_native_ui_token_rotl64(uint64_t value, unsigned int shift) {
    return (value << shift) | (value >> (64u - shift));
}

static uint64_t kain_native_ui_token_nonzero_bit(uint64_t value) {
    return ((value | (UINT64_C(0) - value)) >> 63u) & UINT64_C(1);
}

static uint64_t kain_native_ui_token_zero_bit(uint64_t value) {
    return kain_native_ui_token_nonzero_bit(value) ^ UINT64_C(1);
}

static uint64_t kain_native_ui_token_load_le64(const unsigned char* bytes) {
    return ((uint64_t)bytes[0]) |
        ((uint64_t)bytes[1] << 8u) |
        ((uint64_t)bytes[2] << 16u) |
        ((uint64_t)bytes[3] << 24u) |
        ((uint64_t)bytes[4] << 32u) |
        ((uint64_t)bytes[5] << 40u) |
        ((uint64_t)bytes[6] << 48u) |
        ((uint64_t)bytes[7] << 56u);
}

static uint64_t kain_native_ui_token_state16(uint64_t lo, uint64_t hi, uint64_t length) {
    const uint64_t magic = UINT64_C(0x64170d358aa115a1);
    uint64_t folded0 = (lo ^ length) * magic;
    uint64_t folded1 = (hi ^ kain_native_ui_token_rotl64(magic, 17u)) *
        UINT64_C(0x9e3779b97f4a7c15);
    uint64_t folded2 = ((lo >> 7u) ^ (hi << 11u) ^ UINT64_C(0xbf58476d1ce4e5b9)) *
        UINT64_C(0xd6e8feb86659fd93);
    uint64_t state = folded0 ^ folded1 ^ folded2;
    return ((state ^ (state >> 33u)) * UINT64_C(0xff51afd7ed558ccd)) ^
        (state >> 29u);
}

static KainNativeUiToken16 kain_native_ui_token_from_text16(const char* text) {
    unsigned char bytes[16] = {0};
    KainNativeUiToken16 token;
    size_t length = text ? strlen(text) : 0u;
    size_t copy_length = length;
    if (copy_length > sizeof(bytes)) {
        copy_length = sizeof(bytes);
    }
    if (text && copy_length != 0u) {
        memcpy(bytes, text, copy_length);
    }
    token.length = (uint64_t)length;
    token.lo = kain_native_ui_token_load_le64(bytes);
    token.hi = kain_native_ui_token_load_le64(bytes + 8);
    token.state = kain_native_ui_token_state16(token.lo, token.hi, token.length);
    return token;
}

static uint64_t kain_native_ui_token_match_bit(
    const KainNativeUiToken16* token,
    uint64_t length,
    uint64_t lo,
    uint64_t hi,
    uint64_t state
) {
    return kain_native_ui_token_zero_bit(token->length ^ length) &
        kain_native_ui_token_zero_bit(token->lo ^ lo) &
        kain_native_ui_token_zero_bit(token->hi ^ hi) &
        kain_native_ui_token_zero_bit(token->state ^ state);
}

static KainNativeUiFlagInfo kain_native_ui_flag_info(const char* flag) {
    KainNativeUiToken16 token = kain_native_ui_token_from_text16(flag);
    KainNativeUiFlagInfo info;
    uint64_t hidden = kain_native_ui_token_match_bit(&token, 6u, UINT64_C(0x00006e6564646968), UINT64_C(0x0000000000000000), UINT64_C(0x85daa81451a55c7a));
    uint64_t visible = kain_native_ui_token_match_bit(&token, 7u, UINT64_C(0x00656c6269736976), UINT64_C(0x0000000000000000), UINT64_C(0x7f0f01206f964b92));
    uint64_t focusable = kain_native_ui_token_match_bit(&token, 9u, UINT64_C(0x6c62617375636f66), UINT64_C(0x0000000000000065), UINT64_C(0x7a75024eba4e101f));
    uint64_t interactive = kain_native_ui_token_match_bit(&token, 11u, UINT64_C(0x7463617265746e69), UINT64_C(0x0000000000657669), UINT64_C(0x948038e6c1c6ea72));
    uint64_t disabled = kain_native_ui_token_match_bit(&token, 8u, UINT64_C(0x64656c6261736964), UINT64_C(0x0000000000000000), UINT64_C(0x4f87286f47c95184));
    uint64_t hovered = kain_native_ui_token_match_bit(&token, 7u, UINT64_C(0x0064657265766f68), UINT64_C(0x0000000000000000), UINT64_C(0x13bef354dde61301));
    uint64_t pressed = kain_native_ui_token_match_bit(&token, 7u, UINT64_C(0x0064657373657270), UINT64_C(0x0000000000000000), UINT64_C(0x61f59c74a54f9887));
    info.bit = (int64_t)(
        ((hidden | visible) * (uint64_t)KAIN_NATIVE_UI_NODE_HIDDEN) |
        (focusable * (uint64_t)KAIN_NATIVE_UI_NODE_FOCUSABLE) |
        (interactive * (uint64_t)KAIN_NATIVE_UI_NODE_INTERACTIVE) |
        (disabled * (uint64_t)KAIN_NATIVE_UI_NODE_DISABLED) |
        (hovered * (uint64_t)KAIN_NATIVE_UI_NODE_HOVERED) |
        (pressed * (uint64_t)KAIN_NATIVE_UI_NODE_PRESSED)
    );
    info.visible = visible;
    return info;
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

static KainNativeUiStateRecord* kain_native_ui_find_state(KainNativeUiSession* session, int64_t node_id, const char* key) {
    int64_t index;
    if (!session || !key) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_STATE; index += 1) {
        if (session->state[index].in_use &&
            session->state[index].node_id == node_id &&
            strcmp(session->state[index].key, key) == 0) {
            return &session->state[index];
        }
    }
    return NULL;
}

static KainNativeUiStateRecord* kain_native_ui_ensure_state(KainNativeUiSession* session, int64_t node_id, const char* key) {
    int64_t index;
    KainNativeUiStateRecord* existing = kain_native_ui_find_state(session, node_id, key);
    if (existing) {
        return existing;
    }
    if (!session || !key || session->state_count >= KAIN_NATIVE_UI_MAX_STATE) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_STATE; index += 1) {
        if (!session->state[index].in_use) {
            memset(&session->state[index], 0, sizeof(session->state[index]));
            session->state[index].in_use = 1;
            session->state[index].node_id = node_id;
            kain_native_ui_copy_text(session->state[index].key, sizeof(session->state[index].key), key);
            session->state_count += 1;
            return &session->state[index];
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

static int kain_native_ui_hex_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

static int64_t kain_native_ui_decode_hex(const char* bytes_hex, uint8_t** out_bytes) {
    size_t index;
    size_t length;
    size_t byte_count;
    uint8_t* bytes;
    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (!bytes_hex) {
        return -1;
    }
    length = strlen(bytes_hex);
    if ((length % 2u) != 0u) {
        return -1;
    }
    
    /* Z3 Proved: Cap length to prevent integer overflow and heap corruption */
    if (length > 268435456u) { /* 256 MB max hex string length for 128 MB textures */
        return -1;
    }
    for (index = 0; index < length; index += 1) {
        if (kain_native_ui_hex_value(bytes_hex[index]) < 0) {
            return -1;
        }
    }
    byte_count = length / 2u;
    if (byte_count == 0u) {
        return 0;
    }
    bytes = (uint8_t*)malloc(byte_count);
    if (!bytes) {
        return -1;
    }
    for (index = 0; index < byte_count; index += 1) {
        int high = kain_native_ui_hex_value(bytes_hex[index * 2u]);
        int low = kain_native_ui_hex_value(bytes_hex[index * 2u + 1u]);
        bytes[index] = (uint8_t)((high << 4) | low);
    }
    if (out_bytes) {
        *out_bytes = bytes;
    } else {
        free(bytes);
    }
    return (int64_t)byte_count;
}

static void kain_native_ui_release_resource_bytes(KainNativeUiResource* resource) {
    if (!resource) {
        return;
    }
    if (resource->bytes) {
        free(resource->bytes);
        resource->bytes = NULL;
    }
    resource->byte_length = 0;
}

static void kain_native_ui_release_session_resources(KainNativeUiSession* session) {
    int64_t index;
    if (!session) {
        return;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_RESOURCES; index += 1) {
        if (session->resources[index].in_use) {
            kain_native_ui_release_resource_bytes(&session->resources[index]);
        }
    }
}

static void kain_native_ui_release_session(KainNativeUiSession* session) {
    if (!session) {
        return;
    }
    kain_native_ui_host_adapter_shutdown(session);
    kain_native_ui_release_session_resources(session);
}

int64_t kain_native_ui_reset(void) {
    int64_t index;
    for (index = 0; index < KAIN_NATIVE_UI_MAX_SESSIONS; index += 1) {
        if (g_sessions[index].in_use) {
            kain_native_ui_release_session(&g_sessions[index]);
        }
    }
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
            g_sessions[index].next_resource_id = 1;
            g_sessions[index].next_menu_id = 1;
            g_sessions[index].next_menu_item_id = 1;
            g_sessions[index].next_dialog_id = 1;
            kain_native_ui_copy_text(g_sessions[index].host_backend, sizeof(g_sessions[index].host_backend), "memory");
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
    kain_native_ui_release_session(session);
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
    session->host_should_close = 1;
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

int64_t kain_native_ui_host_attach(int64_t session_id, const char* backend_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    const char* requested_backend;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    requested_backend = (backend_id && backend_id[0]) ? backend_id : "software";
    if (strcmp(requested_backend, "software") == 0) {
        session->host_attached = 1;
        kain_native_ui_copy_text(session->host_backend, sizeof(session->host_backend), "software");
        return KAIN_NATIVE_UI_OK;
    }
    if (kain_native_ui_host_adapter_attach(session, requested_backend) != KAIN_NATIVE_UI_OK) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    session->host_attached = 1;
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_host_pump(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    int64_t adapter_status;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    session->host_pump_count += 1;
    if (kain_native_ui_host_adapter_is_live_backend(session->host_backend)) {
        adapter_status = kain_native_ui_host_adapter_pump(session);
        if (adapter_status != KAIN_NATIVE_UI_OK) {
            return adapter_status;
        }
    }
    return session->event_count;
}

int64_t kain_native_ui_host_present(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    uint64_t hash = UINT64_C(1469598103934665603);
    int64_t index;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    for (index = 0; index < session->draw_command_count; index += 1) {
        KainNativeUiDrawCommand* command = &session->draw_commands[index];
        hash = kain_native_ui_hash_text(hash, command->kind);
        hash = kain_native_ui_hash_i64(hash, command->node_id);
        hash = kain_native_ui_hash_i64(hash, command->resource_id);
        hash = kain_native_ui_hash_i64(hash, command->font_resource_id);
        hash = kain_native_ui_hash_f64(hash, command->x);
        hash = kain_native_ui_hash_f64(hash, command->y);
        hash = kain_native_ui_hash_f64(hash, command->width);
        hash = kain_native_ui_hash_f64(hash, command->height);
        hash = kain_native_ui_hash_text(hash, command->text);
        hash = kain_native_ui_hash_text(hash, command->style_key);
    }
    session->host_attached = session->host_attached ? session->host_attached : 1;
    session->host_presented_draw_count = session->draw_command_count;
    session->host_frame_hash = kain_native_ui_positive_hash(hash);
    session->last_presented_frame = session->frame_index;
    session->dirty_count = 0;
    if (kain_native_ui_host_adapter_is_live_backend(session->host_backend)) {
        int64_t adapter_status = kain_native_ui_host_adapter_present(session);
        if (adapter_status != KAIN_NATIVE_UI_OK) {
            return adapter_status;
        }
    }
    return session->host_presented_draw_count;
}

int64_t kain_native_ui_host_presented_draw_count(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->host_presented_draw_count : KAIN_NATIVE_UI_INVALID_SESSION;
}

int64_t kain_native_ui_host_frame_hash(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->host_frame_hash : KAIN_NATIVE_UI_INVALID_SESSION;
}

int64_t kain_native_ui_host_should_close(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    return (!session->open || session->host_should_close) ? 1 : 0;
}

const char* kain_native_ui_host_backend(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return kain_native_ui_return_string(session ? session->host_backend : g_empty_string);
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

int64_t kain_native_ui_node_set_stable_key(int64_t session_id, int64_t node_id, const char* stable_key) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    kain_native_ui_copy_text(node->stable_key, sizeof(node->stable_key), stable_key);
    kain_native_ui_touch_node(session, node, 8);
    return KAIN_NATIVE_UI_OK;
}

const char* kain_native_ui_node_stable_key(int64_t session_id, int64_t node_id) {
    KainNativeUiNode* node = kain_native_ui_find_node(kain_native_ui_find_session(session_id), node_id);
    return kain_native_ui_return_string(node ? node->stable_key : g_empty_string);
}

int64_t kain_native_ui_node_find_by_stable_key(int64_t session_id, const char* stable_key) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    int64_t index;
    if (!session || !stable_key || !stable_key[0]) {
        return 0;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_NODES; index += 1) {
        if (session->nodes[index].in_use && strcmp(session->nodes[index].stable_key, stable_key) == 0) {
            return session->nodes[index].id;
        }
    }
    return 0;
}

int64_t kain_native_ui_accessibility_set_role(int64_t session_id, int64_t node_id, const char* role) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    kain_native_ui_copy_text(node->accessibility_role, sizeof(node->accessibility_role), role);
    kain_native_ui_touch_node(session, node, 9);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_accessibility_set_label(int64_t session_id, int64_t node_id, const char* label) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    kain_native_ui_copy_text(node->accessibility_label, sizeof(node->accessibility_label), label);
    kain_native_ui_touch_node(session, node, 9);
    return KAIN_NATIVE_UI_OK;
}

const char* kain_native_ui_accessibility_role(int64_t session_id, int64_t node_id) {
    KainNativeUiNode* node = kain_native_ui_find_node(kain_native_ui_find_session(session_id), node_id);
    return kain_native_ui_return_string(node ? node->accessibility_role : g_empty_string);
}

const char* kain_native_ui_accessibility_label(int64_t session_id, int64_t node_id) {
    KainNativeUiNode* node = kain_native_ui_find_node(kain_native_ui_find_session(session_id), node_id);
    return kain_native_ui_return_string(node ? node->accessibility_label : g_empty_string);
}

int64_t kain_native_ui_node_set_flag(int64_t session_id, int64_t node_id, const char* flag, int64_t enabled) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    KainNativeUiFlagInfo flag_info = kain_native_ui_flag_info(flag);
    uint64_t bit_mask = (uint64_t)flag_info.bit;
    uint64_t enabled_bit = kain_native_ui_token_nonzero_bit((uint64_t)enabled) ^ flag_info.visible;
    uint64_t enabled_mask = UINT64_C(0) - enabled_bit;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    if (flag_info.bit == 0) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    node->flags = (int64_t)(((uint64_t)node->flags & ~bit_mask) | (bit_mask & enabled_mask));
    kain_native_ui_touch_node(session, node, 5);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_node_has_flag(int64_t session_id, int64_t node_id, const char* flag) {
    KainNativeUiNode* node = kain_native_ui_find_node(kain_native_ui_find_session(session_id), node_id);
    KainNativeUiFlagInfo flag_info = kain_native_ui_flag_info(flag);
    if (!node || flag_info.bit == 0) {
        return 0;
    }
    return (int64_t)(
        kain_native_ui_token_nonzero_bit((uint64_t)node->flags & (uint64_t)flag_info.bit) ^
        flag_info.visible
    );
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

int64_t kain_native_ui_node_set_state_i64(int64_t session_id, int64_t node_id, const char* key, int64_t value) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiStateRecord* record;
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    record = kain_native_ui_ensure_state(session, node_id, key);
    if (!record) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    record->value_kind = KAIN_NATIVE_UI_STYLE_I64;
    record->i64_value = value;
    kain_native_ui_touch_node(session, node, 12);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_node_set_state_f64(int64_t session_id, int64_t node_id, const char* key, double value) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiStateRecord* record;
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    record = kain_native_ui_ensure_state(session, node_id, key);
    if (!record) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    record->value_kind = KAIN_NATIVE_UI_STYLE_F64;
    record->f64_value = value;
    kain_native_ui_touch_node(session, node, 12);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_node_set_state_string(int64_t session_id, int64_t node_id, const char* key, const char* value) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiStateRecord* record;
    KainNativeUiNode* node = kain_native_ui_find_node(session, node_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!node) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    record = kain_native_ui_ensure_state(session, node_id, key);
    if (!record) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    record->value_kind = KAIN_NATIVE_UI_STYLE_STRING;
    kain_native_ui_copy_text(record->string_value, sizeof(record->string_value), value);
    kain_native_ui_touch_node(session, node, 12);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_node_state_i64(int64_t session_id, int64_t node_id, const char* key, int64_t fallback) {
    KainNativeUiStateRecord* record = kain_native_ui_find_state(kain_native_ui_find_session(session_id), node_id, key);
    return (record && record->value_kind == KAIN_NATIVE_UI_STYLE_I64) ? record->i64_value : fallback;
}

double kain_native_ui_node_state_f64(int64_t session_id, int64_t node_id, const char* key, double fallback) {
    KainNativeUiStateRecord* record = kain_native_ui_find_state(kain_native_ui_find_session(session_id), node_id, key);
    return (record && record->value_kind == KAIN_NATIVE_UI_STYLE_F64) ? record->f64_value : fallback;
}

const char* kain_native_ui_node_state_string(int64_t session_id, int64_t node_id, const char* key, const char* fallback) {
    KainNativeUiStateRecord* record = kain_native_ui_find_state(kain_native_ui_find_session(session_id), node_id, key);
    if (record && record->value_kind == KAIN_NATIVE_UI_STYLE_STRING) {
        return kain_native_ui_return_string(record->string_value);
    }
    return kain_native_ui_return_string(fallback ? fallback : g_empty_string);
}

int64_t kain_native_ui_state_count(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->state_count : KAIN_NATIVE_UI_INVALID_SESSION;
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

int64_t kain_native_ui_resource_create(
    int64_t session_id,
    const char* resource_type,
    const char* key,
    int64_t width,
    int64_t height,
    int64_t byte_length
) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    int64_t index;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!resource_type || !resource_type[0]) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    if (session->resource_count >= KAIN_NATIVE_UI_MAX_RESOURCES) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_RESOURCES; index += 1) {
        if (!session->resources[index].in_use) {
            KainNativeUiResource* resource = &session->resources[index];
            memset(resource, 0, sizeof(*resource));
            resource->in_use = 1;
            resource->id = session->next_resource_id++;
            resource->width = width;
            resource->height = height;
            resource->byte_length = byte_length;
            kain_native_ui_copy_text(resource->resource_type, sizeof(resource->resource_type), resource_type);
            kain_native_ui_copy_text(resource->key, sizeof(resource->key), key);
            session->resource_count += 1;
            return resource->id;
        }
    }
    return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
}

int64_t kain_native_ui_font_create(int64_t session_id, const char* key, const char* family, double size) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    int64_t resource_id = kain_native_ui_resource_create(session_id, "font", key, 0, 0, 0);
    KainNativeUiResource* resource = kain_native_ui_find_resource(session, resource_id);
    if (!resource) {
        return resource_id;
    }
    resource->scalar_value = size > 0.0 ? size : 14.0;
    kain_native_ui_copy_text(resource->aux, sizeof(resource->aux), family);
    return resource_id;
}

int64_t kain_native_ui_texture_create(
    int64_t session_id,
    const char* key,
    int64_t width,
    int64_t height,
    const char* format,
    int64_t byte_length
) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    int64_t resource_id = kain_native_ui_resource_create(session_id, "texture", key, width, height, byte_length);
    KainNativeUiResource* resource = kain_native_ui_find_resource(session, resource_id);
    if (!resource) {
        return resource_id;
    }
    kain_native_ui_copy_text(resource->aux, sizeof(resource->aux), format);
    return resource_id;
}

int64_t kain_native_ui_canvas_create(int64_t session_id, const char* key, int64_t width, int64_t height) {
    return kain_native_ui_resource_create(session_id, "canvas", key, width, height, 0);
}

int64_t kain_native_ui_shader_create(int64_t session_id, const char* key, const char* stage, int64_t byte_length) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    int64_t resource_id = kain_native_ui_resource_create(session_id, "shader", key, 0, 0, byte_length);
    KainNativeUiResource* resource = kain_native_ui_find_resource(session, resource_id);
    if (!resource) {
        return resource_id;
    }
    kain_native_ui_copy_text(resource->aux, sizeof(resource->aux), stage);
    return resource_id;
}

int64_t kain_native_ui_resource_set_bytes(
    int64_t session_id,
    int64_t resource_id,
    const uint8_t* bytes,
    int64_t byte_length
) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiResource* resource = kain_native_ui_find_resource(session, resource_id);
    uint8_t* copy = NULL;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!resource) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    if (byte_length < 0) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    if (byte_length > 0) {
        if (!bytes) {
            return KAIN_NATIVE_UI_INVALID_ARGUMENT;
        }
        copy = (uint8_t*)malloc((size_t)byte_length);
        if (!copy) {
            return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
        }
        memcpy(copy, bytes, (size_t)byte_length);
    }
    kain_native_ui_release_resource_bytes(resource);
    resource->bytes = copy;
    resource->byte_length = byte_length;
    resource->bytes_revision += 1;
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_resource_set_bytes_hex(
    int64_t session_id,
    int64_t resource_id,
    const char* bytes_hex
) {
    uint8_t* decoded = NULL;
    int64_t decoded_length = kain_native_ui_decode_hex(bytes_hex, &decoded);
    int64_t status;
    if (decoded_length < 0) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    status = kain_native_ui_resource_set_bytes(session_id, resource_id, decoded, decoded_length);
    free(decoded);
    return status;
}

int64_t kain_native_ui_resource_count(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->resource_count : KAIN_NATIVE_UI_INVALID_SESSION;
}

int64_t kain_native_ui_resource_exists(int64_t session_id, int64_t resource_id) {
    return kain_native_ui_find_resource(kain_native_ui_find_session(session_id), resource_id) ? 1 : 0;
}

const char* kain_native_ui_resource_type(int64_t session_id, int64_t resource_id) {
    KainNativeUiResource* resource = kain_native_ui_find_resource(kain_native_ui_find_session(session_id), resource_id);
    return kain_native_ui_return_string(resource ? resource->resource_type : g_empty_string);
}

const char* kain_native_ui_resource_key(int64_t session_id, int64_t resource_id) {
    KainNativeUiResource* resource = kain_native_ui_find_resource(kain_native_ui_find_session(session_id), resource_id);
    return kain_native_ui_return_string(resource ? resource->key : g_empty_string);
}

int64_t kain_native_ui_resource_width(int64_t session_id, int64_t resource_id) {
    KainNativeUiResource* resource = kain_native_ui_find_resource(kain_native_ui_find_session(session_id), resource_id);
    return resource ? resource->width : KAIN_NATIVE_UI_INVALID_ARGUMENT;
}

int64_t kain_native_ui_resource_height(int64_t session_id, int64_t resource_id) {
    KainNativeUiResource* resource = kain_native_ui_find_resource(kain_native_ui_find_session(session_id), resource_id);
    return resource ? resource->height : KAIN_NATIVE_UI_INVALID_ARGUMENT;
}

int64_t kain_native_ui_resource_byte_length(int64_t session_id, int64_t resource_id) {
    KainNativeUiResource* resource = kain_native_ui_find_resource(kain_native_ui_find_session(session_id), resource_id);
    return resource ? resource->byte_length : KAIN_NATIVE_UI_INVALID_ARGUMENT;
}

double kain_native_ui_text_measure_width(int64_t session_id, int64_t font_resource_id, const char* text) {
    KainNativeUiResource* resource = kain_native_ui_find_resource(kain_native_ui_find_session(session_id), font_resource_id);
    double size = (resource && resource->scalar_value > 0.0) ? resource->scalar_value : 14.0;
    size_t length = strlen(text ? text : g_empty_string);
    return ((double)length) * size * 0.56;
}

double kain_native_ui_text_measure_height(int64_t session_id, int64_t font_resource_id, const char* text) {
    KainNativeUiResource* resource = kain_native_ui_find_resource(kain_native_ui_find_session(session_id), font_resource_id);
    double size = (resource && resource->scalar_value > 0.0) ? resource->scalar_value : 14.0;
    (void)text;
    return size * 1.25;
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
    int64_t font_resource_id,
    double x,
    double y,
    const char* text,
    const char* style_key
) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiDrawCommand* command;
    KainNativeUiResource* font;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!kain_native_ui_find_node(session, node_id)) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    font = kain_native_ui_find_resource(session, font_resource_id);
    if (!font || strcmp(font->resource_type, "font") != 0) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    command = kain_native_ui_append_draw_command(session, "text");
    if (!command) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    command->node_id = node_id;
    command->font_resource_id = font_resource_id;
    command->x = x;
    command->y = y;
    kain_native_ui_copy_text(command->text, sizeof(command->text), text);
    kain_native_ui_copy_text(command->style_key, sizeof(command->style_key), style_key);
    return session->draw_command_count;
}

int64_t kain_native_ui_draw_resource(
    int64_t session_id,
    int64_t node_id,
    int64_t resource_id,
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
    if (!kain_native_ui_find_resource(session, resource_id)) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    command = kain_native_ui_append_draw_command(session, "resource");
    if (!command) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    command->node_id = node_id;
    command->resource_id = resource_id;
    command->x = x;
    command->y = y;
    command->width = width;
    command->height = height;
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
    KainNativeUiDrawCommand* command = kain_native_ui_find_draw_command(session, command_index);
    if (!command) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    return command->node_id;
}

int64_t kain_native_ui_draw_command_resource(int64_t session_id, int64_t command_index) {
    KainNativeUiDrawCommand* command = kain_native_ui_find_draw_command(kain_native_ui_find_session(session_id), command_index);
    return command ? command->resource_id : KAIN_NATIVE_UI_INVALID_ARGUMENT;
}

double kain_native_ui_draw_command_x(int64_t session_id, int64_t command_index) {
    KainNativeUiDrawCommand* command = kain_native_ui_find_draw_command(kain_native_ui_find_session(session_id), command_index);
    return command ? command->x : 0.0;
}

double kain_native_ui_draw_command_y(int64_t session_id, int64_t command_index) {
    KainNativeUiDrawCommand* command = kain_native_ui_find_draw_command(kain_native_ui_find_session(session_id), command_index);
    return command ? command->y : 0.0;
}

double kain_native_ui_draw_command_width(int64_t session_id, int64_t command_index) {
    KainNativeUiDrawCommand* command = kain_native_ui_find_draw_command(kain_native_ui_find_session(session_id), command_index);
    return command ? command->width : 0.0;
}

double kain_native_ui_draw_command_height(int64_t session_id, int64_t command_index) {
    KainNativeUiDrawCommand* command = kain_native_ui_find_draw_command(kain_native_ui_find_session(session_id), command_index);
    return command ? command->height : 0.0;
}

const char* kain_native_ui_draw_command_text(int64_t session_id, int64_t command_index) {
    KainNativeUiDrawCommand* command = kain_native_ui_find_draw_command(kain_native_ui_find_session(session_id), command_index);
    return kain_native_ui_return_string(command ? command->text : g_empty_string);
}

const char* kain_native_ui_draw_command_style(int64_t session_id, int64_t command_index) {
    KainNativeUiDrawCommand* command = kain_native_ui_find_draw_command(kain_native_ui_find_session(session_id), command_index);
    return kain_native_ui_return_string(command ? command->style_key : g_empty_string);
}

int64_t kain_native_ui_draw_command_font(int64_t session_id, int64_t command_index) {
    KainNativeUiDrawCommand* command = kain_native_ui_find_draw_command(kain_native_ui_find_session(session_id), command_index);
    return command ? command->font_resource_id : KAIN_NATIVE_UI_INVALID_ARGUMENT;
}

int64_t kain_native_ui_clipboard_set_text(int64_t session_id, const char* text) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    kain_native_ui_copy_text(session->clipboard_text, sizeof(session->clipboard_text), text);
    if (kain_native_ui_host_adapter_is_live_backend(session->host_backend)) {
        kain_native_ui_host_adapter_clipboard_set_text(session, session->clipboard_text);
    }
    return KAIN_NATIVE_UI_OK;
}

const char* kain_native_ui_clipboard_text(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (session && kain_native_ui_host_adapter_is_live_backend(session->host_backend)) {
        char clipboard_text[KAIN_NATIVE_UI_MAX_TEXT];
        if (kain_native_ui_host_adapter_clipboard_get_text(
                session,
                clipboard_text,
                sizeof(clipboard_text)
            )) {
            kain_native_ui_copy_text(
                session->clipboard_text,
                sizeof(session->clipboard_text),
                clipboard_text
            );
        }
    }
    return kain_native_ui_return_string(session ? session->clipboard_text : g_empty_string);
}

int64_t kain_native_ui_ime_begin(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!kain_native_ui_find_node(session, node_id)) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    session->ime_active_node_id = node_id;
    session->ime_text[0] = '\0';
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_ime_commit_text(int64_t session_id, const char* text) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (session->ime_active_node_id <= 0) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    kain_native_ui_copy_text(session->ime_text, sizeof(session->ime_text), text);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_ime_end(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    session->ime_active_node_id = 0;
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_ime_active_node(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->ime_active_node_id : KAIN_NATIVE_UI_INVALID_SESSION;
}

const char* kain_native_ui_ime_text(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return kain_native_ui_return_string(session ? session->ime_text : g_empty_string);
}

int64_t kain_native_ui_drag_begin(int64_t session_id, int64_t node_id, const char* payload, double x, double y) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!kain_native_ui_find_node(session, node_id)) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    session->drag_active_node_id = node_id;
    session->drag_drop_target_id = 0;
    session->drag_x = x;
    session->drag_y = y;
    kain_native_ui_copy_text(session->drag_payload, sizeof(session->drag_payload), payload);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_drag_update(int64_t session_id, double x, double y, int64_t drop_target_node_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (session->drag_active_node_id <= 0) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    if (drop_target_node_id > 0 && !kain_native_ui_find_node(session, drop_target_node_id)) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    session->drag_x = x;
    session->drag_y = y;
    session->drag_drop_target_id = drop_target_node_id;
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_drag_drop(int64_t session_id, int64_t drop_target_node_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (session->drag_active_node_id <= 0) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    if (drop_target_node_id > 0 && !kain_native_ui_find_node(session, drop_target_node_id)) {
        return KAIN_NATIVE_UI_INVALID_NODE;
    }
    session->drag_drop_target_id = drop_target_node_id;
    return drop_target_node_id;
}

int64_t kain_native_ui_drag_active_node(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->drag_active_node_id : KAIN_NATIVE_UI_INVALID_SESSION;
}

int64_t kain_native_ui_drag_drop_target(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->drag_drop_target_id : KAIN_NATIVE_UI_INVALID_SESSION;
}

double kain_native_ui_drag_x(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->drag_x : 0.0;
}

double kain_native_ui_drag_y(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->drag_y : 0.0;
}

const char* kain_native_ui_drag_payload(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return kain_native_ui_return_string(session ? session->drag_payload : g_empty_string);
}

int64_t kain_native_ui_menu_create(int64_t session_id, const char* key) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    int64_t index;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (session->menu_count >= KAIN_NATIVE_UI_MAX_MENUS) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_MENUS; index += 1) {
        if (!session->menus[index].in_use) {
            KainNativeUiMenu* menu = &session->menus[index];
            memset(menu, 0, sizeof(*menu));
            menu->in_use = 1;
            menu->id = session->next_menu_id++;
            kain_native_ui_copy_text(menu->key, sizeof(menu->key), key);
            session->menu_count += 1;
            return menu->id;
        }
    }
    return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
}

int64_t kain_native_ui_menu_add_item(
    int64_t session_id,
    int64_t menu_id,
    const char* key,
    const char* label,
    int64_t command_id
) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiMenu* menu = kain_native_ui_find_menu(session, menu_id);
    int64_t index;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!menu) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    if (session->menu_item_count >= KAIN_NATIVE_UI_MAX_MENU_ITEMS) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_MENU_ITEMS; index += 1) {
        if (!session->menu_items[index].in_use) {
            KainNativeUiMenuItem* item = &session->menu_items[index];
            memset(item, 0, sizeof(*item));
            item->in_use = 1;
            item->id = session->next_menu_item_id++;
            item->menu_id = menu_id;
            item->command_id = command_id;
            kain_native_ui_copy_text(item->key, sizeof(item->key), key);
            kain_native_ui_copy_text(item->label, sizeof(item->label), label);
            menu->item_count += 1;
            session->menu_item_count += 1;
            return item->id;
        }
    }
    return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
}

int64_t kain_native_ui_menu_open(int64_t session_id, int64_t menu_id, double x, double y) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiMenu* menu = kain_native_ui_find_menu(session, menu_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!menu) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    menu->open = 1;
    menu->x = x;
    menu->y = y;
    session->active_menu_id = menu_id;
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_menu_active(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->active_menu_id : KAIN_NATIVE_UI_INVALID_SESSION;
}

int64_t kain_native_ui_menu_item_count(int64_t session_id, int64_t menu_id) {
    KainNativeUiMenu* menu = kain_native_ui_find_menu(kain_native_ui_find_session(session_id), menu_id);
    return menu ? menu->item_count : KAIN_NATIVE_UI_INVALID_ARGUMENT;
}

const char* kain_native_ui_menu_item_label(int64_t session_id, int64_t menu_id, int64_t item_index) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    int64_t index;
    int64_t seen = 0;
    if (!session || item_index < 0) {
        return kain_native_ui_return_string(g_empty_string);
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_MENU_ITEMS; index += 1) {
        if (session->menu_items[index].in_use && session->menu_items[index].menu_id == menu_id) {
            if (seen == item_index) {
                return kain_native_ui_return_string(session->menu_items[index].label);
            }
            seen += 1;
        }
    }
    return kain_native_ui_return_string(g_empty_string);
}

int64_t kain_native_ui_menu_item_command(int64_t session_id, int64_t menu_id, int64_t item_index) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    int64_t index;
    int64_t seen = 0;
    if (!session || item_index < 0) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_MENU_ITEMS; index += 1) {
        if (session->menu_items[index].in_use && session->menu_items[index].menu_id == menu_id) {
            if (seen == item_index) {
                return session->menu_items[index].command_id;
            }
            seen += 1;
        }
    }
    return KAIN_NATIVE_UI_INVALID_ARGUMENT;
}

int64_t kain_native_ui_dialog_request(int64_t session_id, const char* kind, const char* title, const char* message) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    int64_t index;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (session->dialog_count >= KAIN_NATIVE_UI_MAX_DIALOGS) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_DIALOGS; index += 1) {
        if (!session->dialogs[index].in_use) {
            KainNativeUiDialog* dialog = &session->dialogs[index];
            memset(dialog, 0, sizeof(*dialog));
            dialog->in_use = 1;
            dialog->id = session->next_dialog_id++;
            kain_native_ui_copy_text(dialog->kind, sizeof(dialog->kind), kind);
            kain_native_ui_copy_text(dialog->title, sizeof(dialog->title), title);
            kain_native_ui_copy_text(dialog->message, sizeof(dialog->message), message);
            session->dialog_count += 1;
            session->active_dialog_id = dialog->id;
            return dialog->id;
        }
    }
    return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
}

int64_t kain_native_ui_dialog_active(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->active_dialog_id : KAIN_NATIVE_UI_INVALID_SESSION;
}

const char* kain_native_ui_dialog_kind(int64_t session_id, int64_t dialog_id) {
    KainNativeUiDialog* dialog = kain_native_ui_find_dialog(kain_native_ui_find_session(session_id), dialog_id);
    return kain_native_ui_return_string(dialog ? dialog->kind : g_empty_string);
}

const char* kain_native_ui_dialog_title(int64_t session_id, int64_t dialog_id) {
    KainNativeUiDialog* dialog = kain_native_ui_find_dialog(kain_native_ui_find_session(session_id), dialog_id);
    return kain_native_ui_return_string(dialog ? dialog->title : g_empty_string);
}

const char* kain_native_ui_dialog_message(int64_t session_id, int64_t dialog_id) {
    KainNativeUiDialog* dialog = kain_native_ui_find_dialog(kain_native_ui_find_session(session_id), dialog_id);
    return kain_native_ui_return_string(dialog ? dialog->message : g_empty_string);
}

int64_t kain_native_ui_dialog_respond(int64_t session_id, int64_t dialog_id, int64_t result, const char* response_text) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    KainNativeUiDialog* dialog = kain_native_ui_find_dialog(session, dialog_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!dialog) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    dialog->result = result;
    dialog->response_ready = 1;
    kain_native_ui_copy_text(dialog->response_text, sizeof(dialog->response_text), response_text);
    session->dialog_response_ready = 1;
    session->dialog_response_result = result;
    kain_native_ui_copy_text(session->dialog_response_text, sizeof(session->dialog_response_text), response_text);
    if (session->active_dialog_id == dialog_id) {
        session->active_dialog_id = 0;
    }
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_dialog_poll_response(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    int64_t result;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!session->dialog_response_ready) {
        return 0;
    }
    result = session->dialog_response_result;
    session->dialog_response_ready = 0;
    return result;
}

const char* kain_native_ui_dialog_response_text(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return kain_native_ui_return_string(session ? session->dialog_response_text : g_empty_string);
}

int64_t kain_native_ui_hot_reload_begin(int64_t session_id, const char* revision_key) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    session->hot_reload_generation += 1;
    kain_native_ui_copy_text(session->hot_reload_key, sizeof(session->hot_reload_key), revision_key);
    return session->hot_reload_generation;
}

int64_t kain_native_ui_hot_reload_commit(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    return session->hot_reload_generation;
}

int64_t kain_native_ui_hot_reload_generation(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return session ? session->hot_reload_generation : KAIN_NATIVE_UI_INVALID_SESSION;
}

const char* kain_native_ui_hot_reload_key(int64_t session_id) {
    KainNativeUiSession* session = kain_native_ui_find_session(session_id);
    return kain_native_ui_return_string(session ? session->hot_reload_key : g_empty_string);
}
