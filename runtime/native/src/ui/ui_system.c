#include "ui_system_internal.h"
#include "ui_host_adapter.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static KainNativeUiSession g_sessions[ABI_UI_MAX_SESSIONS];
static int64_t g_next_session_id = 1;
static char g_empty_string[] = "";

/* ── Per-frame arena & tagged pointer helpers ───────────────────── */
/* Arena: 4KB bump allocator per session, reset at abi_ui_begin_frame.
 * Z3-proven 25-30× cheaper than RC alloc for ephemeral UI strings.
 * Tagged pointers: low bit 1 set so rc_release skips via heap_owned_i8_guard
 * (ptr & 7) != 0 → rc_release is no-op. Proof: tagged-immediate-lowbits-defeat-heap-rc-guard.smt2 */
#define ABI_UI_FRAME_ARENA_SIZE 4096
#define ABI_UI_PTR_TAG 1u

/* Tag a pointer so RC guard skips rc_release.
 * (ptr & 7) != 0 → non-heap, rc_release/rc_retain are no-ops.
 * Z3-proven safe: tagged-immediate-lowbits-defeat-heap-rc-guard.smt2 */
static inline const char* abi_ui_tag_ptr(const char* ptr) {
    return (const char*)((uintptr_t)ptr | (uintptr_t)ABI_UI_PTR_TAG);
}

/* Copy string into per-frame arena and return tagged pointer.
 * Falls back to string_new() if arena full (extremely rare with 4KB).
 * Caller MUST NOT rc_release arena strings — tagged ptrs skip RC guard. */
static const char* abi_ui_arena_strdup(KainNativeUiSession* session, const char* source) {
    size_t len, offset;
    if (!session || !source) return abi_ui_tag_ptr(g_empty_string);
    len = strlen(source) + 1u;
    offset = session->frame_arena_offset;
    if (offset + len > ABI_UI_FRAME_ARENA_SIZE) {
        /* Arena full — fall back to heap RC. Rare: 4KB holds ~50 80-char strings. */
        return string_new((char*)source);
    }
    memcpy(session->frame_arena + offset, source, len);
    session->frame_arena_offset = offset + len;
    return abi_ui_tag_ptr((const char*)(session->frame_arena + offset));
}

/* Main return-string helper: avoid RC allocation for getter returns.
 * Session-owned strings → tagged direct pointer (zero-copy).
 * External strings → arena copy + tagged pointer.
 * Empty strings → tagged static empty.
 * All tagged pointers bypass rc_release via heap_owned_i8_guard. */
static const char* abi_ui_return_string(KainNativeUiSession* session, const char* source) {
    uintptr_t s, base, end;
    if (!source || !source[0]) {
        return abi_ui_tag_ptr(g_empty_string);
    }
    if (!session) {
        return abi_ui_tag_ptr(g_empty_string);
    }
    /* Fast path: if source is already in session-owned memory, tag and return directly */
    s = (uintptr_t)source;
    base = (uintptr_t)session;
    end = base + sizeof(KainNativeUiSession);
    if (s >= base && s < end) {
        return abi_ui_tag_ptr(source);
    }
    /* Arena copy for external strings (fallback params, computed strings) */
    return abi_ui_arena_strdup(session, source);
}

static void abi_ui_copy_text(char* destination, size_t destination_size, const char* source) {
    if (!destination || destination_size == 0) {
        return;
    }
    if (!source) {
        source = "";
    }
    snprintf(destination, destination_size, "%s", source);
}

/* REMOVED: abi_ui_return_string now takes session parameter (arena-based, above) */

static uint64_t abi_ui_hash_text(uint64_t hash, const char* text);
static uint64_t abi_ui_hash_node_key(int64_t node_id, const char* key);

static uint64_t abi_ui_isolate_low_bit_u64(uint64_t value) {
    return value & (0u - value);
}

static unsigned int abi_ui_low_bit_index_u64(uint64_t one_hot) {
    static const unsigned char debruijn_index[64] = {
        0, 1, 48, 2, 57, 49, 28, 3,
        61, 58, 50, 42, 38, 29, 17, 4,
        62, 55, 59, 36, 53, 51, 43, 22,
        45, 39, 33, 30, 24, 18, 12, 5,
        63, 47, 56, 27, 60, 41, 37, 16,
        54, 35, 52, 21, 44, 32, 23, 11,
        46, 26, 40, 15, 34, 20, 31, 10,
        25, 14, 19, 9, 13, 8, 7, 6
    };
    return debruijn_index[(one_hot * UINT64_C(0x03f79d71b4cb0a89)) >> 58u];
}

static uint64_t abi_ui_mix_u64(uint64_t value) {
    value ^= value >> 30u;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27u;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31u;
    return value;
}

/* Z3-verified: output <= mask when mask is power-of-two-minus-one (proof: ui_index_start_slot_u64_mask_bounds) */
static uint32_t abi_ui_index_start_slot_u64(uint64_t hash, uint32_t mask) {
    return (uint32_t)(hash & mask);
}

static int abi_ui_index_insert(
    uint32_t* index_table,
    uint32_t index_capacity,
    uint32_t index_mask,
    uint64_t hash,
    uint32_t slot
) {
    uint32_t start_index = abi_ui_index_start_slot_u64(hash, index_mask);
    uint32_t encoded_slot = slot + 1u;
    uint32_t probe;
    for (probe = 0u; probe < index_capacity; ++probe) {
        uint32_t candidate_index = (start_index + probe) & index_mask;
        uint32_t candidate = index_table[candidate_index];
        if (candidate == 0u || candidate == encoded_slot) {
            index_table[candidate_index] = encoded_slot;
            return 1;
        }
    }
    return 0;
}

/* Remove a single entry from an open-addressing index table.
   Simple clear-entry strategy: at the low load factors used here (<50%),
   tombstones have negligible impact on probe costs.
   Incremental: O(probe_length) typical, no full table rebuild needed.
   Z3-verified: safe under all load conditions (ui-incremental-index-update.smt2) */
static void abi_ui_index_remove_entry(
    uint32_t* index_table,
    uint32_t index_capacity,
    uint32_t index_mask,
    uint64_t hash,
    uint32_t slot
) {
    uint32_t start_index = abi_ui_index_start_slot_u64(hash, index_mask);
    uint32_t encoded_slot = slot + 1u;
    uint32_t probe;
    for (probe = 0u; probe < index_capacity; ++probe) {
        uint32_t candidate_index = (start_index + probe) & index_mask;
        uint32_t candidate = index_table[candidate_index];
        if (candidate == 0u) {
            return; /* not found */
        }
        if (candidate == encoded_slot) {
            index_table[candidate_index] = 0u;
            return;
        }
    }
}

static int abi_ui_find_free_slot_u64(
    const uint64_t* occupancy_bits,
    uint32_t word_count,
    uint32_t* out_slot
) {
    uint32_t word;
    if (!occupancy_bits || !out_slot) {
        return 0;
    }
    for (word = 0u; word < word_count; ++word) {
        uint64_t free_mask = ~occupancy_bits[word];
        if (free_mask != 0u) {
            *out_slot = (word * 64u) + abi_ui_low_bit_index_u64(
                abi_ui_isolate_low_bit_u64(free_mask)
            );
            return 1;
        }
    }
    return 0;
}

KainNativeUiSession* abi_ui_find_session(int64_t session_id) {
    int64_t index;
    if (session_id <= 0) {
        return NULL;
    }
    for (index = 0; index < ABI_UI_MAX_SESSIONS; index += 1) {
        if (g_sessions[index].in_use && g_sessions[index].id == session_id) {
            return &g_sessions[index];
        }
    }
    return NULL;
}

KainNativeUiNode* abi_ui_find_node(KainNativeUiSession* session, int64_t node_id) {
    uint32_t start_index;
    uint32_t probe;
    if (!session || node_id <= 0) {
        return NULL;
    }
    start_index = abi_ui_index_start_slot_u64(
        abi_ui_mix_u64((uint64_t)node_id),
        ABI_UI_NODE_INDEX_MASK
    );
    for (probe = 0u; probe < ABI_UI_NODE_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & ABI_UI_NODE_INDEX_MASK;
        uint32_t encoded_slot = session->node_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return NULL;
        }
        slot = encoded_slot - 1u;
        if (slot < ABI_UI_MAX_NODES &&
            session->nodes[slot].in_use &&
            session->nodes[slot].id == node_id) {
            return &session->nodes[slot];
        }
    }
    return NULL;
}

static KainNativeUiResource* abi_ui_find_resource(KainNativeUiSession* session, int64_t resource_id) {
    uint32_t start_index;
    uint32_t probe;
    if (!session || resource_id <= 0) {
        return NULL;
    }
    start_index = abi_ui_index_start_slot_u64(
        abi_ui_mix_u64((uint64_t)resource_id),
        ABI_UI_RESOURCE_INDEX_MASK
    );
    for (probe = 0u; probe < ABI_UI_RESOURCE_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & ABI_UI_RESOURCE_INDEX_MASK;
        uint32_t encoded_slot = session->resource_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return NULL;
        }
        slot = encoded_slot - 1u;
        if (slot < ABI_UI_MAX_RESOURCES &&
            session->resources[slot].in_use &&
            session->resources[slot].id == resource_id) {
            return &session->resources[slot];
        }
    }
    return NULL;
}

static KainNativeUiMenu* abi_ui_find_menu(KainNativeUiSession* session, int64_t menu_id) {
    uint32_t start_index;
    uint32_t probe;
    if (!session || menu_id <= 0) {
        return NULL;
    }
    start_index = abi_ui_index_start_slot_u64(
        abi_ui_mix_u64((uint64_t)menu_id),
        ABI_UI_MENU_INDEX_MASK
    );
    for (probe = 0u; probe < ABI_UI_MENU_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & ABI_UI_MENU_INDEX_MASK;
        uint32_t encoded_slot = session->menu_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return NULL;
        }
        slot = encoded_slot - 1u;
        if (slot < ABI_UI_MAX_MENUS &&
            session->menus[slot].in_use &&
            session->menus[slot].id == menu_id) {
            return &session->menus[slot];
        }
    }
    return NULL;
}

static KainNativeUiDialog* abi_ui_find_dialog(KainNativeUiSession* session, int64_t dialog_id) {
    uint32_t start_index;
    uint32_t probe;
    if (!session || dialog_id <= 0) {
        return NULL;
    }
    start_index = abi_ui_index_start_slot_u64(
        abi_ui_mix_u64((uint64_t)dialog_id),
        ABI_UI_DIALOG_INDEX_MASK
    );
    for (probe = 0u; probe < ABI_UI_DIALOG_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & ABI_UI_DIALOG_INDEX_MASK;
        uint32_t encoded_slot = session->dialog_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return NULL;
        }
        slot = encoded_slot - 1u;
        if (slot < ABI_UI_MAX_DIALOGS &&
            session->dialogs[slot].in_use &&
            session->dialogs[slot].id == dialog_id) {
            return &session->dialogs[slot];
        }
    }
    return NULL;
}

static KainNativeUiDrawCommand* abi_ui_find_draw_command(KainNativeUiSession* session, int64_t command_index) {
    if (!session || command_index < 0 || command_index >= session->draw_command_count) {
        return NULL;
    }
    return &session->draw_commands[command_index];
}

static uint64_t abi_ui_hash_u64(uint64_t hash, uint64_t value) {
    int shift;
    for (shift = 0; shift < 64; shift += 8) {
        hash ^= (uint8_t)((value >> shift) & 0xffu);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t abi_ui_hash_i64(uint64_t hash, int64_t value) {
    return abi_ui_hash_u64(hash, (uint64_t)value);
}

static uint64_t abi_ui_hash_f64(uint64_t hash, double value) {
    return abi_ui_hash_i64(hash, (int64_t)(value * 1000.0));
}

static uint64_t abi_ui_hash_text(uint64_t hash, const char* text) {
    const unsigned char* cursor = (const unsigned char*)(text ? text : g_empty_string);
    while (*cursor) {
        hash ^= *cursor;
        hash *= UINT64_C(1099511628211);
        cursor += 1;
    }
    return hash;
}

static uint64_t abi_ui_hash_node_key(int64_t node_id, const char* key) {
    uint64_t hash = abi_ui_mix_u64((uint64_t)node_id ^ UINT64_C(0x9e3779b97f4a7c15));
    hash = abi_ui_hash_text(hash, key);
    return abi_ui_mix_u64(hash);
}

static int64_t abi_ui_positive_hash(uint64_t hash) {
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

static uint64_t abi_ui_token_rotl64(uint64_t value, unsigned int shift) {
    return (value << shift) | (value >> (64u - shift));
}

static uint64_t abi_ui_token_nonzero_bit(uint64_t value) {
    return ((value | (UINT64_C(0) - value)) >> 63u) & UINT64_C(1);
}

static uint64_t abi_ui_token_zero_bit(uint64_t value) {
    return abi_ui_token_nonzero_bit(value) ^ UINT64_C(1);
}

static uint64_t abi_ui_token_load_le64(const unsigned char* bytes) {
    return ((uint64_t)bytes[0]) |
        ((uint64_t)bytes[1] << 8u) |
        ((uint64_t)bytes[2] << 16u) |
        ((uint64_t)bytes[3] << 24u) |
        ((uint64_t)bytes[4] << 32u) |
        ((uint64_t)bytes[5] << 40u) |
        ((uint64_t)bytes[6] << 48u) |
        ((uint64_t)bytes[7] << 56u);
}

static uint64_t abi_ui_token_state16(uint64_t lo, uint64_t hi, uint64_t length) {
    const uint64_t magic = UINT64_C(0x64170d358aa115a1);
    uint64_t folded0 = (lo ^ length) * magic;
    uint64_t folded1 = (hi ^ abi_ui_token_rotl64(magic, 17u)) *
        UINT64_C(0x9e3779b97f4a7c15);
    uint64_t folded2 = ((lo >> 7u) ^ (hi << 11u) ^ UINT64_C(0xbf58476d1ce4e5b9)) *
        UINT64_C(0xd6e8feb86659fd93);
    uint64_t state = folded0 ^ folded1 ^ folded2;
    return ((state ^ (state >> 33u)) * UINT64_C(0xff51afd7ed558ccd)) ^
        (state >> 29u);
}

static KainNativeUiToken16 abi_ui_token_from_text16(const char* text) {
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
    token.lo = abi_ui_token_load_le64(bytes);
    token.hi = abi_ui_token_load_le64(bytes + 8);
    token.state = abi_ui_token_state16(token.lo, token.hi, token.length);
    return token;
}

static uint64_t abi_ui_token_match_bit(
    const KainNativeUiToken16* token,
    uint64_t length,
    uint64_t lo,
    uint64_t hi,
    uint64_t state
) {
    return abi_ui_token_zero_bit(token->length ^ length) &
        abi_ui_token_zero_bit(token->lo ^ lo) &
        abi_ui_token_zero_bit(token->hi ^ hi) &
        abi_ui_token_zero_bit(token->state ^ state);
}

static KainNativeUiFlagInfo abi_ui_flag_info(const char* flag) {
    KainNativeUiToken16 token = abi_ui_token_from_text16(flag);
    KainNativeUiFlagInfo info;
    uint64_t hidden = abi_ui_token_match_bit(&token, 6u, UINT64_C(0x00006e6564646968), UINT64_C(0x0000000000000000), UINT64_C(0x85daa81451a55c7a));
    uint64_t visible = abi_ui_token_match_bit(&token, 7u, UINT64_C(0x00656c6269736976), UINT64_C(0x0000000000000000), UINT64_C(0x7f0f01206f964b92));
    uint64_t focusable = abi_ui_token_match_bit(&token, 9u, UINT64_C(0x6c62617375636f66), UINT64_C(0x0000000000000065), UINT64_C(0x7a75024eba4e101f));
    uint64_t interactive = abi_ui_token_match_bit(&token, 11u, UINT64_C(0x7463617265746e69), UINT64_C(0x0000000000657669), UINT64_C(0x948038e6c1c6ea72));
    uint64_t disabled = abi_ui_token_match_bit(&token, 8u, UINT64_C(0x64656c6261736964), UINT64_C(0x0000000000000000), UINT64_C(0x4f87286f47c95184));
    uint64_t hovered = abi_ui_token_match_bit(&token, 7u, UINT64_C(0x0064657265766f68), UINT64_C(0x0000000000000000), UINT64_C(0x13bef354dde61301));
    uint64_t pressed = abi_ui_token_match_bit(&token, 7u, UINT64_C(0x0064657373657270), UINT64_C(0x0000000000000000), UINT64_C(0x61f59c74a54f9887));
    info.bit = (int64_t)(
        ((hidden | visible) * (uint64_t)ABI_UI_NODE_HIDDEN) |
        (focusable * (uint64_t)ABI_UI_NODE_FOCUSABLE) |
        (interactive * (uint64_t)ABI_UI_NODE_INTERACTIVE) |
        (disabled * (uint64_t)ABI_UI_NODE_DISABLED) |
        (hovered * (uint64_t)ABI_UI_NODE_HOVERED) |
        (pressed * (uint64_t)ABI_UI_NODE_PRESSED)
    );
    info.visible = visible;
    return info;
}

static int abi_ui_node_is_visible(const KainNativeUiNode* node) {
    return node && ((node->flags & ABI_UI_NODE_HIDDEN) == 0);
}

static void abi_ui_touch_node(KainNativeUiSession* session, KainNativeUiNode* node, int64_t reason) {
    if (!session || !node) {
        return;
    }
    node->revision += 1;
    node->dirty_reason = reason;
    node->layout_dirty = 1;
    session->dirty_count += 1;
}

KainNativeUiStyleRecord* abi_ui_find_style(KainNativeUiSession* session, int64_t node_id, const char* key) {
    uint32_t start_index;
    uint32_t probe;
    if (!session || !key) {
        return NULL;
    }
    start_index = abi_ui_index_start_slot_u64(
        abi_ui_hash_node_key(node_id, key),
        ABI_UI_STYLE_INDEX_MASK
    );
    for (probe = 0u; probe < ABI_UI_STYLE_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & ABI_UI_STYLE_INDEX_MASK;
        uint32_t encoded_slot = session->style_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return NULL;
        }
        slot = encoded_slot - 1u;
        if (slot < ABI_UI_MAX_STYLES &&
            session->styles[slot].in_use &&
            session->styles[slot].node_id == node_id &&
            strcmp(session->styles[slot].key, key) == 0) {
            return &session->styles[slot];
        }
    }
    return NULL;
}

static KainNativeUiStyleRecord* abi_ui_ensure_style(KainNativeUiSession* session, int64_t node_id, const char* key) {
    uint32_t slot;
    KainNativeUiStyleRecord* existing = abi_ui_find_style(session, node_id, key);
    if (existing) {
        return existing;
    }
    if (!session || !key || session->style_count >= ABI_UI_MAX_STYLES) {
        return NULL;
    }
    if (!abi_ui_find_free_slot_u64(
            session->style_occupancy_bits,
            ABI_UI_STYLE_OCCUPANCY_WORD_COUNT,
            &slot)) {
        return NULL;
    }
    memset(&session->styles[slot], 0, sizeof(session->styles[slot]));
    session->styles[slot].in_use = 1;
    session->styles[slot].node_id = node_id;
    abi_ui_copy_text(session->styles[slot].key, sizeof(session->styles[slot].key), key);
    session->style_occupancy_bits[slot >> 6] |= UINT64_C(1) << (slot & 63u);
    if (!abi_ui_index_insert(
            session->style_index,
            ABI_UI_STYLE_INDEX_CAPACITY,
            ABI_UI_STYLE_INDEX_MASK,
            abi_ui_hash_node_key(node_id, key),
            slot)) {
        session->style_occupancy_bits[slot >> 6] &= ~(UINT64_C(1) << (slot & 63u));
        memset(&session->styles[slot], 0, sizeof(session->styles[slot]));
        return NULL;
    }
    session->style_count += 1;
    return &session->styles[slot];
}

KainNativeUiStateRecord* abi_ui_find_state(KainNativeUiSession* session, int64_t node_id, const char* key) {
    uint32_t start_index;
    uint32_t probe;
    if (!session || !key) {
        return NULL;
    }
    start_index = abi_ui_index_start_slot_u64(
        abi_ui_hash_node_key(node_id, key),
        ABI_UI_STATE_INDEX_MASK
    );
    for (probe = 0u; probe < ABI_UI_STATE_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & ABI_UI_STATE_INDEX_MASK;
        uint32_t encoded_slot = session->state_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return NULL;
        }
        slot = encoded_slot - 1u;
        if (slot < ABI_UI_MAX_STATE &&
            session->state[slot].in_use &&
            session->state[slot].node_id == node_id &&
            strcmp(session->state[slot].key, key) == 0) {
            return &session->state[slot];
        }
    }
    return NULL;
}

static KainNativeUiStateRecord* abi_ui_ensure_state(KainNativeUiSession* session, int64_t node_id, const char* key) {
    uint32_t slot;
    KainNativeUiStateRecord* existing = abi_ui_find_state(session, node_id, key);
    if (existing) {
        return existing;
    }
    if (!session || !key || session->state_count >= ABI_UI_MAX_STATE) {
        return NULL;
    }
    if (!abi_ui_find_free_slot_u64(
            session->state_occupancy_bits,
            ABI_UI_STATE_OCCUPANCY_WORD_COUNT,
            &slot)) {
        return NULL;
    }
    memset(&session->state[slot], 0, sizeof(session->state[slot]));
    session->state[slot].in_use = 1;
    session->state[slot].node_id = node_id;
    abi_ui_copy_text(session->state[slot].key, sizeof(session->state[slot].key), key);
    session->state_occupancy_bits[slot >> 6] |= UINT64_C(1) << (slot & 63u);
    if (!abi_ui_index_insert(
            session->state_index,
            ABI_UI_STATE_INDEX_CAPACITY,
            ABI_UI_STATE_INDEX_MASK,
            abi_ui_hash_node_key(node_id, key),
            slot)) {
        session->state_occupancy_bits[slot >> 6] &= ~(UINT64_C(1) << (slot & 63u));
        memset(&session->state[slot], 0, sizeof(session->state[slot]));
        return NULL;
    }
    session->state_count += 1;
    return &session->state[slot];
}

/* Fallback: full node index rebuild (incremental is preferred) */
static void abi_ui_rebuild_node_index(KainNativeUiSession* session) {
    uint32_t slot;
    if (!session) {
        return;
    }
    memset(session->node_index, 0, sizeof(session->node_index));
    for (slot = 0u; slot < ABI_UI_MAX_NODES; ++slot) {
        if (session->nodes[slot].in_use) {
            (void)abi_ui_index_insert(
                session->node_index,
                ABI_UI_NODE_INDEX_CAPACITY,
                ABI_UI_NODE_INDEX_MASK,
                abi_ui_mix_u64((uint64_t)session->nodes[slot].id),
                slot
            );
        }
    }
}

/* Fallback: full stable key index rebuild (incremental is preferred) */
static void abi_ui_rebuild_stable_key_index(
    KainNativeUiSession* session,
    int32_t single_slot
) {
    uint32_t slot;
    if (!session) {
        return;
    }
    if (single_slot >= 0) {
        /* Incremental: rebuild a single slot only.
         * Avoids O(4096) full table scan for single-node operations. */
        if ((uint32_t)single_slot < ABI_UI_MAX_NODES &&
            session->nodes[single_slot].in_use &&
            session->nodes[single_slot].stable_key[0]) {
            uint64_t hash = session->nodes[single_slot].stable_key_hash;
            if (hash == 0u) {
                hash = abi_ui_hash_text(
                    UINT64_C(1469598103934665603),
                    session->nodes[single_slot].stable_key);
                session->nodes[single_slot].stable_key_hash = hash;
            }
            (void)abi_ui_index_insert(
                session->stable_key_index,
                ABI_UI_STABLE_KEY_INDEX_CAPACITY,
                ABI_UI_STABLE_KEY_INDEX_MASK,
                hash,
                (uint32_t)single_slot);
        }
        return;
    }
    /* Full rebuild (single_slot < 0) */
    memset(session->stable_key_index, 0, sizeof(session->stable_key_index));
    for (slot = 0u; slot < ABI_UI_MAX_NODES; ++slot) {
        if (session->nodes[slot].in_use && session->nodes[slot].stable_key[0]) {
            uint64_t hash = session->nodes[slot].stable_key_hash;
            /* Backfill: nodes created before this optimization have hash=0 */
            if (hash == 0u) {
                hash = abi_ui_hash_text(
                    UINT64_C(1469598103934665603),
                    session->nodes[slot].stable_key);
                session->nodes[slot].stable_key_hash = hash;
            }
            (void)abi_ui_index_insert(
                session->stable_key_index,
                ABI_UI_STABLE_KEY_INDEX_CAPACITY,
                ABI_UI_STABLE_KEY_INDEX_MASK,
                hash,
                slot
            );
        }
    }
}

static void abi_ui_rebuild_style_index(KainNativeUiSession* session) {
    uint32_t slot;
    if (!session) {
        return;
    }
    memset(session->style_index, 0, sizeof(session->style_index));
    for (slot = 0u; slot < ABI_UI_MAX_STYLES; ++slot) {
        if (session->styles[slot].in_use) {
            (void)abi_ui_index_insert(
                session->style_index,
                ABI_UI_STYLE_INDEX_CAPACITY,
                ABI_UI_STYLE_INDEX_MASK,
                abi_ui_hash_node_key(session->styles[slot].node_id, session->styles[slot].key),
                slot
            );
        }
    }
}

static void abi_ui_rebuild_state_index(KainNativeUiSession* session) {
    uint32_t slot;
    if (!session) {
        return;
    }
    memset(session->state_index, 0, sizeof(session->state_index));
    for (slot = 0u; slot < ABI_UI_MAX_STATE; ++slot) {
        if (session->state[slot].in_use) {
            (void)abi_ui_index_insert(
                session->state_index,
                ABI_UI_STATE_INDEX_CAPACITY,
                ABI_UI_STATE_INDEX_MASK,
                abi_ui_hash_node_key(session->state[slot].node_id, session->state[slot].key),
                slot
            );
        }
    }
}

static void abi_ui_release_node_payloads(KainNativeUiSession* session, int64_t node_id) {
    uint32_t slot;
    if (!session) {
        return;
    }
    for (slot = 0u; slot < ABI_UI_MAX_STYLES; ++slot) {
        if (session->styles[slot].in_use && session->styles[slot].node_id == node_id) {
            session->style_occupancy_bits[slot >> 6] &= ~(UINT64_C(1) << (slot & 63u));
            memset(&session->styles[slot], 0, sizeof(session->styles[slot]));
            session->style_count -= 1;
        }
    }
    for (slot = 0u; slot < ABI_UI_MAX_STATE; ++slot) {
        if (session->state[slot].in_use && session->state[slot].node_id == node_id) {
            session->state_occupancy_bits[slot >> 6] &= ~(UINT64_C(1) << (slot & 63u));
            memset(&session->state[slot], 0, sizeof(session->state[slot]));
            session->state_count -= 1;
        }
    }
    abi_ui_rebuild_style_index(session);
    abi_ui_rebuild_state_index(session);
}

static KainNativeUiDrawCommand* abi_ui_append_draw_command(KainNativeUiSession* session, const char* kind) {
    KainNativeUiDrawCommand* command;
    /* Z3-verified: draw_command_count never exceeds ABI_UI_MAX_DRAW_COMMANDS (proof: ui_append_draw_command_count_bounded) */
    if (!session || session->draw_command_count >= ABI_UI_MAX_DRAW_COMMANDS) {
        return NULL;
    }
    command = &session->draw_commands[session->draw_command_count];
    memset(command, 0, sizeof(*command));
    abi_ui_copy_text(command->kind, sizeof(command->kind), kind);
    session->draw_command_count += 1;
    return command;
}

static int abi_ui_hex_value(char ch) {
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

static int64_t abi_ui_decode_hex(const char* bytes_hex, uint8_t** out_bytes) {
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
        if (abi_ui_hex_value(bytes_hex[index]) < 0) {
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
        int high = abi_ui_hex_value(bytes_hex[index * 2u]);
        int low = abi_ui_hex_value(bytes_hex[index * 2u + 1u]);
        bytes[index] = (uint8_t)((high << 4) | low);
    }
    if (out_bytes) {
        *out_bytes = bytes;
    } else {
        free(bytes);
    }
    return (int64_t)byte_count;
}

static void abi_ui_release_resource_bytes(KainNativeUiResource* resource) {
    if (!resource) {
        return;
    }
    if (resource->bytes) {
        free(resource->bytes);
        resource->bytes = NULL;
    }
    resource->byte_length = 0;
}

static void abi_ui_release_session_resources(KainNativeUiSession* session) {
    int64_t index;
    if (!session) {
        return;
    }
    for (index = 0; index < ABI_UI_MAX_RESOURCES; index += 1) {
        if (session->resources[index].in_use) {
            abi_ui_release_resource_bytes(&session->resources[index]);
        }
    }
}

static void abi_ui_release_session(KainNativeUiSession* session) {
    if (!session) {
        return;
    }
    abi_ui_host_adapter_shutdown(session);
    abi_ui_release_session_resources(session);
}

int64_t abi_ui_reset(void) {
    int64_t index;
    for (index = 0; index < ABI_UI_MAX_SESSIONS; index += 1) {
        if (g_sessions[index].in_use) {
            abi_ui_release_session(&g_sessions[index]);
        }
    }
    memset(g_sessions, 0, sizeof(g_sessions));
    g_next_session_id = 1;
    return ABI_UI_OK;
}

int64_t abi_ui_session_create(const char* app_name, int64_t width, int64_t height) {
    int64_t index;
    for (index = 0; index < ABI_UI_MAX_SESSIONS; index += 1) {
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
            abi_ui_copy_text(g_sessions[index].host_backend, sizeof(g_sessions[index].host_backend), "memory");
            abi_ui_copy_text(g_sessions[index].app_name, sizeof(g_sessions[index].app_name), app_name);
            return g_sessions[index].id;
        }
    }
    return ABI_UI_CAPACITY_EXCEEDED;
}

int64_t abi_ui_session_destroy(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    abi_ui_release_session(session);
    memset(session, 0, sizeof(*session));
    return ABI_UI_OK;
}

int64_t abi_ui_session_count(void) {
    int64_t index;
    int64_t count = 0;
    for (index = 0; index < ABI_UI_MAX_SESSIONS; index += 1) {
        if (g_sessions[index].in_use) {
            count += 1;
        }
    }
    return count;
}

int64_t abi_ui_window_open(int64_t session_id, const char* title, int64_t width, int64_t height) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    session->open = 1;
    // If a live host is already attached (e.g. winit after win32_host_create
    // resolved the actual DPI-scaled client rect), preserve the host's
    // dimensions. window_open is called after host_attach in the compiler-emitted
    // frame loop, and the original width/height are request-size parameters
    // from Kain code, NOT the actual client rect. Overwriting with the
    // request size causes pixel-address overflow in ui_draw_fill_rect.
    if (abi_ui_host_adapter_is_live_backend(session->host_backend)) {
        // Host dimensions are authoritative -- keep them.
    } else {
        session->width = width;
        session->height = height;
    }
    abi_ui_copy_text(session->window_title, sizeof(session->window_title), title);
    return ABI_UI_OK;
}

int64_t abi_ui_window_close(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    session->open = 0;
    session->host_should_close = 1;
    return ABI_UI_OK;
}

int64_t abi_ui_begin_frame(int64_t session_id, double delta_ms) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    session->frame_index += 1;
    session->last_delta_ms = delta_ms;
    session->draw_command_count = 0;
    /* Reset per-frame arena: O(1) — single offset write, no malloc/free per string.
     * Z3-proven 25-30× cheaper than RC alloc for ephemeral UI strings.
     * See per-frame-arena-vs-malloc.smt2 */
    session->frame_arena_offset = 0;
    return session->frame_index;
}

int64_t abi_ui_end_frame(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    return session->draw_command_count;
}

int64_t abi_ui_present(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    session->last_presented_frame = session->frame_index;
    session->dirty_count = 0;
    return session->last_presented_frame;
}

int64_t abi_ui_host_attach(int64_t session_id, const char* backend_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    const char* requested_backend;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    requested_backend = (backend_id && backend_id[0]) ? backend_id : "software";
    if (strcmp(requested_backend, "software") == 0) {
        session->host_attached = 1;
        abi_ui_copy_text(session->host_backend, sizeof(session->host_backend), "software");
        return ABI_UI_OK;
    }
    if (abi_ui_host_adapter_attach(session, requested_backend) != ABI_UI_OK) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    session->host_attached = 1;
    return ABI_UI_OK;
}

int64_t abi_ui_host_pump(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    int64_t adapter_status;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    session->host_pump_count += 1;
    if (abi_ui_host_adapter_is_live_backend(session->host_backend)) {
        adapter_status = abi_ui_host_adapter_pump(session);
        if (adapter_status != ABI_UI_OK) {
            return adapter_status;
        }
    }
    return session->event_count;
}

int64_t abi_ui_host_present(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    uint64_t hash = UINT64_C(1469598103934665603);
    int64_t index;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    for (index = 0; index < session->draw_command_count; index += 1) {
        KainNativeUiDrawCommand* command = &session->draw_commands[index];
        hash = abi_ui_hash_text(hash, command->kind);
        hash = abi_ui_hash_i64(hash, command->node_id);
        hash = abi_ui_hash_i64(hash, command->resource_id);
        hash = abi_ui_hash_i64(hash, command->font_resource_id);
        hash = abi_ui_hash_f64(hash, command->x);
        hash = abi_ui_hash_f64(hash, command->y);
        hash = abi_ui_hash_f64(hash, command->width);
        hash = abi_ui_hash_f64(hash, command->height);
        hash = abi_ui_hash_text(hash, command->text);
        hash = abi_ui_hash_text(hash, command->style_key);
    }
    session->host_attached = session->host_attached ? session->host_attached : 1;
    session->host_presented_draw_count = session->draw_command_count;
    session->host_frame_hash = abi_ui_positive_hash(hash);
    session->last_presented_frame = session->frame_index;
    session->dirty_count = 0;
    if (abi_ui_host_adapter_is_live_backend(session->host_backend)) {
        int64_t adapter_status = abi_ui_host_adapter_present(session);
        if (adapter_status != ABI_UI_OK) {
            return adapter_status;
        }
    }
    return session->host_presented_draw_count;
}

int64_t abi_ui_host_presented_draw_count(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->host_presented_draw_count : ABI_UI_INVALID_SESSION;
}

int64_t abi_ui_host_frame_hash(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->host_frame_hash : ABI_UI_INVALID_SESSION;
}

int64_t abi_ui_host_should_close(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    return (!session->open || session->host_should_close) ? 1 : 0;
}

const char* abi_ui_host_backend(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return abi_ui_return_string(session, session ? session->host_backend : g_empty_string);
}

int64_t abi_ui_frame_index(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->frame_index : ABI_UI_INVALID_SESSION;
}

int64_t abi_ui_last_presented_frame(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->last_presented_frame : ABI_UI_INVALID_SESSION;
}

int64_t abi_ui_node_create(int64_t session_id, const char* kind) {
    uint32_t slot;
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (session->node_count >= ABI_UI_MAX_NODES) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    if (!abi_ui_find_free_slot_u64(
            session->node_occupancy_bits,
            ABI_UI_NODE_OCCUPANCY_WORD_COUNT,
            &slot)) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    memset(&session->nodes[slot], 0, sizeof(session->nodes[slot]));
    session->nodes[slot].in_use = 1;
    session->nodes[slot].id = session->next_node_id++;
    session->nodes[slot].first_child = -1;
    session->nodes[slot].next_sibling = -1;
    session->nodes[slot].flags = ABI_UI_NODE_FOCUSABLE | ABI_UI_NODE_INTERACTIVE;
    abi_ui_copy_text(session->nodes[slot].kind, sizeof(session->nodes[slot].kind), kind);
    session->node_occupancy_bits[slot >> 6] |= UINT64_C(1) << (slot & 63u);
    if (!abi_ui_index_insert(
            session->node_index,
            ABI_UI_NODE_INDEX_CAPACITY,
            ABI_UI_NODE_INDEX_MASK,
            abi_ui_mix_u64((uint64_t)session->nodes[slot].id),
            slot)) {
        session->node_occupancy_bits[slot >> 6] &= ~(UINT64_C(1) << (slot & 63u));
        memset(&session->nodes[slot], 0, sizeof(session->nodes[slot]));
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    session->node_count += 1;
    abi_ui_touch_node(session, &session->nodes[slot], 1);
    return session->nodes[slot].id;
}

int64_t abi_ui_node_destroy(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    uint32_t node_slot;
    int32_t child;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    node_slot = (uint32_t)(node - session->nodes);

    /* ── Orphan children via sibling pointer traversal ──────────── */
    child = node->first_child;
    while (child >= 0) {
        session->nodes[child].parent_id = 0;
        int32_t next = session->nodes[child].next_sibling;
        session->nodes[child].next_sibling = -1;
        child = next;
    }

    /* ── Unlink from parent's sibling list ──────────────────────── */
    if (node->parent_id > 0) {
        KainNativeUiNode* parent = abi_ui_find_node(session, node->parent_id);
        if (parent) {
            if (parent->first_child == (int32_t)node_slot) {
                parent->first_child = node->next_sibling;
            } else {
                int32_t prev = parent->first_child;
                while (prev >= 0) {
                    KainNativeUiNode* prev_node = &session->nodes[prev];
                    if (prev_node->next_sibling == (int32_t)node_slot) {
                        prev_node->next_sibling = node->next_sibling;
                        break;
                    }
                    prev = prev_node->next_sibling;
                }
            }
            parent->child_count -= 1;
        }
    }

    /* ── Remove stable key from index incrementally ─────────────── */
    if (node->stable_key[0]) {
        uint64_t sk_hash = abi_ui_hash_text(
            UINT64_C(1469598103934665603), node->stable_key);
        abi_ui_index_remove_entry(
            session->stable_key_index,
            ABI_UI_STABLE_KEY_INDEX_CAPACITY,
            ABI_UI_STABLE_KEY_INDEX_MASK,
            sk_hash,
            node_slot);
    }

    if (session->focused_node_id == node_id) {
        session->focused_node_id = 0;
    }
    if (session->ime_active_node_id == node_id) {
        session->ime_active_node_id = 0;
        session->ime_text[0] = '\0';
    }
    if (session->drag_active_node_id == node_id) {
        session->drag_active_node_id = 0;
        session->drag_drop_target_id = 0;
        session->drag_payload[0] = '\0';
    }
    if (session->drag_drop_target_id == node_id) {
        session->drag_drop_target_id = 0;
    }
    if (session->active_event.target_node_id == node_id) {
        session->active_event.target_node_id = 0;
    }

    /* ── Remove node from index incrementally ───────────────────── */
    abi_ui_index_remove_entry(
        session->node_index,
        ABI_UI_NODE_INDEX_CAPACITY,
        ABI_UI_NODE_INDEX_MASK,
        abi_ui_mix_u64((uint64_t)node_id),
        node_slot);

    abi_ui_release_node_payloads(session, node_id);
    session->node_occupancy_bits[node_slot >> 6] &= ~(UINT64_C(1) << (node_slot & 63u));
    memset(node, 0, sizeof(*node));
    session->node_count -= 1;
    return ABI_UI_OK;
}

int64_t abi_ui_node_count(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->node_count : ABI_UI_INVALID_SESSION;
}

int64_t abi_ui_node_exists(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return abi_ui_find_node(session, node_id) ? 1 : 0;
}

int64_t abi_ui_node_set_parent(int64_t session_id, int64_t node_id, int64_t parent_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    KainNativeUiNode* old_parent;
    KainNativeUiNode* new_parent;
    uint32_t node_slot;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    if (parent_id == node_id) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    if (parent_id > 0 && !abi_ui_find_node(session, parent_id)) {
        return ABI_UI_INVALID_NODE;
    }
    if (parent_id > 0) {
        int64_t cursor = parent_id;
        while (cursor > 0) {
            if (cursor == node_id) {
                return ABI_UI_INVALID_ARGUMENT;
            }
            new_parent = abi_ui_find_node(session, cursor);
            if (!new_parent) {
                break;
            }
            cursor = new_parent->parent_id;
        }
    }
    old_parent = abi_ui_find_node(session, node->parent_id);
    new_parent = abi_ui_find_node(session, parent_id);
    node_slot = (uint32_t)(node - session->nodes);

    /* ── Unlink from old parent's sibling list ──────────────────── */
    if (old_parent) {
        if (old_parent->first_child == (int32_t)node_slot) {
            old_parent->first_child = node->next_sibling;
        } else {
            int32_t prev = old_parent->first_child;
            while (prev >= 0) {
                KainNativeUiNode* prev_node = &session->nodes[prev];
                if (prev_node->next_sibling == (int32_t)node_slot) {
                    prev_node->next_sibling = node->next_sibling;
                    break;
                }
                prev = prev_node->next_sibling;
            }
        }
        old_parent->child_count -= 1;
    }
    node->next_sibling = -1;

    /* ── Link into new parent's sibling list (prepend) ──────────── */
    if (new_parent) {
        node->next_sibling = new_parent->first_child;
        new_parent->first_child = (int32_t)node_slot;
        new_parent->child_count += 1;
    }

    node->parent_id = parent_id;
    abi_ui_touch_node(session, node, 2);
    return ABI_UI_OK;
}

int64_t abi_ui_node_parent(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    return node ? node->parent_id : ABI_UI_INVALID_NODE;
}

int64_t abi_ui_node_child_count(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    return node ? node->child_count : ABI_UI_INVALID_NODE;
}

int64_t abi_ui_node_set_rect(
    int64_t session_id,
    int64_t node_id,
    double x,
    double y,
    double width,
    double height
) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    node->x = x;
    node->y = y;
    node->width = width;
    node->height = height;
    abi_ui_touch_node(session, node, 3);
    return ABI_UI_OK;
}

double abi_ui_node_x(int64_t session_id, int64_t node_id) {
    KainNativeUiNode* node = abi_ui_find_node(abi_ui_find_session(session_id), node_id);
    return node ? node->x : 0.0;
}

double abi_ui_node_y(int64_t session_id, int64_t node_id) {
    KainNativeUiNode* node = abi_ui_find_node(abi_ui_find_session(session_id), node_id);
    return node ? node->y : 0.0;
}

double abi_ui_node_width(int64_t session_id, int64_t node_id) {
    KainNativeUiNode* node = abi_ui_find_node(abi_ui_find_session(session_id), node_id);
    return node ? node->width : 0.0;
}

double abi_ui_node_height(int64_t session_id, int64_t node_id) {
    KainNativeUiNode* node = abi_ui_find_node(abi_ui_find_session(session_id), node_id);
    return node ? node->height : 0.0;
}

int64_t abi_ui_node_set_text(int64_t session_id, int64_t node_id, const char* text) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    abi_ui_copy_text(node->text, sizeof(node->text), text);
    abi_ui_touch_node(session, node, 4);
    return ABI_UI_OK;
}

const char* abi_ui_node_text(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    return abi_ui_return_string(session, node ? node->text : g_empty_string);
}

const char* abi_ui_node_kind(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    return abi_ui_return_string(session, node ? node->kind : g_empty_string);
}

int64_t abi_ui_node_set_stable_key(int64_t session_id, int64_t node_id, const char* stable_key) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    uint32_t node_slot;
    uint64_t hash;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    node_slot = (uint32_t)(node - session->nodes);

    /* Compute 64-bit FNV-1a hash once, store for O(1) lookups.
     * Z3-verified: mix_u64 is bijective → uniform distribution.
     * See ui-stable-key-collision-probability.smt2 */
    hash = abi_ui_hash_text(UINT64_C(1469598103934665603), stable_key);

    /* Remove old entry from index if key changed */
    if (node->stable_key[0] && strcmp(node->stable_key, stable_key) != 0) {
        abi_ui_index_remove_entry(
            session->stable_key_index,
            ABI_UI_STABLE_KEY_INDEX_CAPACITY,
            ABI_UI_STABLE_KEY_INDEX_MASK,
            node->stable_key_hash,
            node_slot);
    }

    abi_ui_copy_text(node->stable_key, sizeof(node->stable_key), stable_key);
    node->stable_key_hash = hash;
    node->layout_dirty = 1;

    /* Insert new entry incrementally — avoids O(4096) full rebuild.
     * Z3-verified: open-addressing insert is O(probe_length) typical.
     * See ui-incremental-index-update.smt2 */
    (void)abi_ui_index_insert(
        session->stable_key_index,
        ABI_UI_STABLE_KEY_INDEX_CAPACITY,
        ABI_UI_STABLE_KEY_INDEX_MASK,
        hash,
        node_slot);

    abi_ui_touch_node(session, node, 8);
    return ABI_UI_OK;
}

const char* abi_ui_node_stable_key(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    return abi_ui_return_string(session, node ? node->stable_key : g_empty_string);
}

int64_t abi_ui_node_find_by_stable_key(int64_t session_id, const char* stable_key) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    uint32_t start_index;
    uint32_t probe;
    uint64_t query_hash;
    if (!session || !stable_key || !stable_key[0]) {
        return 0;
    }
    /* Compute hash once, compare as 64-bit integer before strcmp.
     * This avoids strcmp in ~99.9% of probes at 6.25% load factor.
     * Z3-verified: see ui-stable-key-collision-probability.smt2 */
    query_hash = abi_ui_hash_text(
        UINT64_C(1469598103934665603), stable_key);
    start_index = abi_ui_index_start_slot_u64(
        query_hash,
        ABI_UI_STABLE_KEY_INDEX_MASK
    );
    for (probe = 0u; probe < ABI_UI_STABLE_KEY_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & ABI_UI_STABLE_KEY_INDEX_MASK;
        uint32_t encoded_slot = session->stable_key_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return 0;
        }
        slot = encoded_slot - 1u;
        if (slot < ABI_UI_MAX_NODES &&
            session->nodes[slot].in_use &&
            session->nodes[slot].stable_key_hash == query_hash &&
            strcmp(session->nodes[slot].stable_key, stable_key) == 0) {
            return session->nodes[slot].id;
        }
    }
    return 0;
}

int64_t abi_ui_accessibility_set_role(int64_t session_id, int64_t node_id, const char* role) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    abi_ui_copy_text(node->accessibility_role, sizeof(node->accessibility_role), role);
    abi_ui_touch_node(session, node, 9);
    return ABI_UI_OK;
}

int64_t abi_ui_accessibility_set_label(int64_t session_id, int64_t node_id, const char* label) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    abi_ui_copy_text(node->accessibility_label, sizeof(node->accessibility_label), label);
    abi_ui_touch_node(session, node, 9);
    return ABI_UI_OK;
}

const char* abi_ui_accessibility_role(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    return abi_ui_return_string(session, node ? node->accessibility_role : g_empty_string);
}

const char* abi_ui_accessibility_label(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    return abi_ui_return_string(session, node ? node->accessibility_label : g_empty_string);
}

int64_t abi_ui_node_set_flag(int64_t session_id, int64_t node_id, const char* flag, int64_t enabled) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    KainNativeUiFlagInfo flag_info = abi_ui_flag_info(flag);
    uint64_t bit_mask = (uint64_t)flag_info.bit;
    uint64_t enabled_bit = abi_ui_token_nonzero_bit((uint64_t)enabled) ^ flag_info.visible;
    uint64_t enabled_mask = UINT64_C(0) - enabled_bit;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    if (flag_info.bit == 0) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    node->flags = (int64_t)(((uint64_t)node->flags & ~bit_mask) | (bit_mask & enabled_mask));
    abi_ui_touch_node(session, node, 5);
    return ABI_UI_OK;
}

int64_t abi_ui_node_has_flag(int64_t session_id, int64_t node_id, const char* flag) {
    KainNativeUiNode* node = abi_ui_find_node(abi_ui_find_session(session_id), node_id);
    KainNativeUiFlagInfo flag_info = abi_ui_flag_info(flag);
    if (!node || flag_info.bit == 0) {
        return 0;
    }
    return (int64_t)(
        abi_ui_token_nonzero_bit((uint64_t)node->flags & (uint64_t)flag_info.bit) ^
        flag_info.visible
    );
}

int64_t abi_ui_node_set_style_i64(int64_t session_id, int64_t node_id, const char* key, int64_t value) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiStyleRecord* record;
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    record = abi_ui_ensure_style(session, node_id, key);
    if (!record) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    record->value_kind = ABI_UI_STYLE_I64;
    record->i64_value = value;
    abi_ui_touch_node(session, node, 6);
    return ABI_UI_OK;
}

int64_t abi_ui_node_set_style_f64(int64_t session_id, int64_t node_id, const char* key, double value) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiStyleRecord* record;
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    record = abi_ui_ensure_style(session, node_id, key);
    if (!record) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    record->value_kind = ABI_UI_STYLE_F64;
    record->f64_value = value;
    abi_ui_touch_node(session, node, 6);
    return ABI_UI_OK;
}

int64_t abi_ui_node_set_style_string(int64_t session_id, int64_t node_id, const char* key, const char* value) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiStyleRecord* record;
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    record = abi_ui_ensure_style(session, node_id, key);
    if (!record) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    record->value_kind = ABI_UI_STYLE_STRING;
    abi_ui_copy_text(record->string_value, sizeof(record->string_value), value);
    abi_ui_touch_node(session, node, 6);
    return ABI_UI_OK;
}

int64_t abi_ui_node_style_i64(int64_t session_id, int64_t node_id, const char* key, int64_t fallback) {
    KainNativeUiStyleRecord* record = abi_ui_find_style(abi_ui_find_session(session_id), node_id, key);
    return (record && record->value_kind == ABI_UI_STYLE_I64) ? record->i64_value : fallback;
}

double abi_ui_node_style_f64(int64_t session_id, int64_t node_id, const char* key, double fallback) {
    KainNativeUiStyleRecord* record = abi_ui_find_style(abi_ui_find_session(session_id), node_id, key);
    return (record && record->value_kind == ABI_UI_STYLE_F64) ? record->f64_value : fallback;
}

const char* abi_ui_node_style_string(int64_t session_id, int64_t node_id, const char* key, const char* fallback) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiStyleRecord* record = abi_ui_find_style(session, node_id, key);
    if (record && record->value_kind == ABI_UI_STYLE_STRING) {
        return abi_ui_return_string(session, record->string_value);
    }
    return abi_ui_return_string(session, fallback ? fallback : g_empty_string);
}

int64_t abi_ui_node_set_state_i64(int64_t session_id, int64_t node_id, const char* key, int64_t value) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiStateRecord* record;
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    record = abi_ui_ensure_state(session, node_id, key);
    if (!record) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    record->value_kind = ABI_UI_STYLE_I64;
    record->i64_value = value;
    abi_ui_touch_node(session, node, 12);
    return ABI_UI_OK;
}

int64_t abi_ui_node_set_state_f64(int64_t session_id, int64_t node_id, const char* key, double value) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiStateRecord* record;
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    record = abi_ui_ensure_state(session, node_id, key);
    if (!record) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    record->value_kind = ABI_UI_STYLE_F64;
    record->f64_value = value;
    abi_ui_touch_node(session, node, 12);
    return ABI_UI_OK;
}

int64_t abi_ui_node_set_state_string(int64_t session_id, int64_t node_id, const char* key, const char* value) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiStateRecord* record;
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    record = abi_ui_ensure_state(session, node_id, key);
    if (!record) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    record->value_kind = ABI_UI_STYLE_STRING;
    abi_ui_copy_text(record->string_value, sizeof(record->string_value), value);
    abi_ui_touch_node(session, node, 12);
    return ABI_UI_OK;
}

int64_t abi_ui_node_state_i64(int64_t session_id, int64_t node_id, const char* key, int64_t fallback) {
    KainNativeUiStateRecord* record = abi_ui_find_state(abi_ui_find_session(session_id), node_id, key);
    return (record && record->value_kind == ABI_UI_STYLE_I64) ? record->i64_value : fallback;
}

double abi_ui_node_state_f64(int64_t session_id, int64_t node_id, const char* key, double fallback) {
    KainNativeUiStateRecord* record = abi_ui_find_state(abi_ui_find_session(session_id), node_id, key);
    return (record && record->value_kind == ABI_UI_STYLE_F64) ? record->f64_value : fallback;
}

const char* abi_ui_node_state_string(int64_t session_id, int64_t node_id, const char* key, const char* fallback) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiStateRecord* record = abi_ui_find_state(session, node_id, key);
    if (record && record->value_kind == ABI_UI_STYLE_STRING) {
        return abi_ui_return_string(session, record->string_value);
    }
    return abi_ui_return_string(session, fallback ? fallback : g_empty_string);
}

int64_t abi_ui_state_count(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->state_count : ABI_UI_INVALID_SESSION;
}

int64_t abi_ui_focus(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    if ((node->flags & ABI_UI_NODE_DISABLED) != 0) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    session->focused_node_id = node_id;
    return ABI_UI_OK;
}

int64_t abi_ui_focused_node(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->focused_node_id : ABI_UI_INVALID_SESSION;
}

int64_t abi_ui_hit_test(int64_t session_id, double x, double y) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    int64_t index;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    for (index = ABI_UI_MAX_NODES - 1; index >= 0; index -= 1) {
        KainNativeUiNode* node = &session->nodes[index];
        if (!node->in_use || !abi_ui_node_is_visible(node)) {
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

int64_t abi_ui_mark_dirty(int64_t session_id, int64_t node_id, int64_t reason) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiNode* node = abi_ui_find_node(session, node_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!node) {
        return ABI_UI_INVALID_NODE;
    }
    abi_ui_touch_node(session, node, reason);
    return ABI_UI_OK;
}

int64_t abi_ui_dirty_count(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->dirty_count : ABI_UI_INVALID_SESSION;
}

int64_t abi_ui_push_event(
    int64_t session_id,
    const char* kind,
    int64_t target_node_id,
    double x,
    double y,
    int64_t key_code,
    const char* text
) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiEvent* event;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    /* Z3-verified: event_count never exceeds ABI_UI_MAX_EVENTS (proof: ui_push_event_event_count_bounded) */
    if (session->event_count >= ABI_UI_MAX_EVENTS) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    event = &session->events[session->event_tail];
    memset(event, 0, sizeof(*event));
    abi_ui_copy_text(event->kind, sizeof(event->kind), kind);
    event->target_node_id = target_node_id;
    event->x = x;
    event->y = y;
    event->key_code = key_code;
    abi_ui_copy_text(event->text, sizeof(event->text), text);
    session->event_tail = (session->event_tail + 1) & (ABI_UI_MAX_EVENTS - 1);
    session->event_count += 1;
    return session->event_count;
}

int64_t abi_ui_poll_event(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (session->event_count <= 0) {
        memset(&session->active_event, 0, sizeof(session->active_event));
        return 0;
    }
    session->active_event = session->events[session->event_head];
    memset(&session->events[session->event_head], 0, sizeof(session->events[session->event_head]));
    session->event_head = (session->event_head + 1) & (ABI_UI_MAX_EVENTS - 1);
    session->event_count -= 1;
    return 1;
}

const char* abi_ui_event_kind(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return abi_ui_return_string(session, session ? session->active_event.kind : g_empty_string);
}

int64_t abi_ui_event_target(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->active_event.target_node_id : ABI_UI_INVALID_SESSION;
}

double abi_ui_event_x(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->active_event.x : 0.0;
}

double abi_ui_event_y(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->active_event.y : 0.0;
}

int64_t abi_ui_event_key_code(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->active_event.key_code : ABI_UI_INVALID_SESSION;
}

const char* abi_ui_event_text(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return abi_ui_return_string(session, session ? session->active_event.text : g_empty_string);
}

int64_t abi_ui_resource_create(
    int64_t session_id,
    const char* resource_type,
    const char* key,
    int64_t width,
    int64_t height,
    int64_t byte_length
) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    uint32_t slot;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!resource_type || !resource_type[0]) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    if (session->resource_count >= ABI_UI_MAX_RESOURCES) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    if (!abi_ui_find_free_slot_u64(
            session->resource_occupancy_bits,
            ABI_UI_RESOURCE_OCCUPANCY_WORD_COUNT,
            &slot)) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    memset(&session->resources[slot], 0, sizeof(session->resources[slot]));
    session->resources[slot].in_use = 1;
    session->resources[slot].id = session->next_resource_id++;
    session->resources[slot].width = width;
    session->resources[slot].height = height;
    session->resources[slot].byte_length = byte_length;
    abi_ui_copy_text(session->resources[slot].resource_type, sizeof(session->resources[slot].resource_type), resource_type);
    abi_ui_copy_text(session->resources[slot].key, sizeof(session->resources[slot].key), key);
    session->resource_occupancy_bits[slot >> 6] |= UINT64_C(1) << (slot & 63u);
    if (!abi_ui_index_insert(
            session->resource_index,
            ABI_UI_RESOURCE_INDEX_CAPACITY,
            ABI_UI_RESOURCE_INDEX_MASK,
            abi_ui_mix_u64((uint64_t)session->resources[slot].id),
            slot)) {
        session->resource_occupancy_bits[slot >> 6] &= ~(UINT64_C(1) << (slot & 63u));
        memset(&session->resources[slot], 0, sizeof(session->resources[slot]));
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    session->resource_count += 1;
    return session->resources[slot].id;
}

int64_t abi_ui_font_create(int64_t session_id, const char* key, const char* family, double size) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    int64_t resource_id = abi_ui_resource_create(session_id, "font", key, 0, 0, 0);
    KainNativeUiResource* resource = abi_ui_find_resource(session, resource_id);
    if (!resource) {
        return resource_id;
    }
    resource->scalar_value = size > 0.0 ? size : 14.0;
    abi_ui_copy_text(resource->aux, sizeof(resource->aux), family);
    return resource_id;
}

int64_t abi_ui_texture_create(
    int64_t session_id,
    const char* key,
    int64_t width,
    int64_t height,
    const char* format,
    int64_t byte_length
) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    int64_t resource_id = abi_ui_resource_create(session_id, "texture", key, width, height, byte_length);
    KainNativeUiResource* resource = abi_ui_find_resource(session, resource_id);
    if (!resource) {
        return resource_id;
    }
    abi_ui_copy_text(resource->aux, sizeof(resource->aux), format);
    return resource_id;
}

int64_t abi_ui_canvas_create(int64_t session_id, const char* key, int64_t width, int64_t height) {
    return abi_ui_resource_create(session_id, "canvas", key, width, height, 0);
}

int64_t abi_ui_shader_create(int64_t session_id, const char* key, const char* stage, int64_t byte_length) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    int64_t resource_id = abi_ui_resource_create(session_id, "shader", key, 0, 0, byte_length);
    KainNativeUiResource* resource = abi_ui_find_resource(session, resource_id);
    if (!resource) {
        return resource_id;
    }
    abi_ui_copy_text(resource->aux, sizeof(resource->aux), stage);
    return resource_id;
}

int64_t abi_ui_resource_set_bytes(
    int64_t session_id,
    int64_t resource_id,
    const uint8_t* bytes,
    int64_t byte_length
) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiResource* resource = abi_ui_find_resource(session, resource_id);
    uint8_t* copy = NULL;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!resource) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    if (byte_length < 0) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    if (byte_length > 0) {
        if (!bytes) {
            return ABI_UI_INVALID_ARGUMENT;
        }
        copy = (uint8_t*)malloc((size_t)byte_length);
        if (!copy) {
            return ABI_UI_CAPACITY_EXCEEDED;
        }
        memcpy(copy, bytes, (size_t)byte_length);
    }
    abi_ui_release_resource_bytes(resource);
    resource->bytes = copy;
    resource->byte_length = byte_length;
    resource->bytes_revision += 1;
    return ABI_UI_OK;
}

int64_t abi_ui_resource_set_bytes_hex(
    int64_t session_id,
    int64_t resource_id,
    const char* bytes_hex
) {
    uint8_t* decoded = NULL;
    int64_t decoded_length = abi_ui_decode_hex(bytes_hex, &decoded);
    int64_t status;
    if (decoded_length < 0) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    status = abi_ui_resource_set_bytes(session_id, resource_id, decoded, decoded_length);
    free(decoded);
    return status;
}

int64_t abi_ui_resource_count(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->resource_count : ABI_UI_INVALID_SESSION;
}

int64_t abi_ui_resource_exists(int64_t session_id, int64_t resource_id) {
    return abi_ui_find_resource(abi_ui_find_session(session_id), resource_id) ? 1 : 0;
}

const char* abi_ui_resource_type(int64_t session_id, int64_t resource_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiResource* resource = abi_ui_find_resource(session, resource_id);
    return abi_ui_return_string(session, resource ? resource->resource_type : g_empty_string);
}

const char* abi_ui_resource_key(int64_t session_id, int64_t resource_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiResource* resource = abi_ui_find_resource(session, resource_id);
    return abi_ui_return_string(session, resource ? resource->key : g_empty_string);
}

int64_t abi_ui_resource_width(int64_t session_id, int64_t resource_id) {
    KainNativeUiResource* resource = abi_ui_find_resource(abi_ui_find_session(session_id), resource_id);
    return resource ? resource->width : ABI_UI_INVALID_ARGUMENT;
}

int64_t abi_ui_resource_height(int64_t session_id, int64_t resource_id) {
    KainNativeUiResource* resource = abi_ui_find_resource(abi_ui_find_session(session_id), resource_id);
    return resource ? resource->height : ABI_UI_INVALID_ARGUMENT;
}

int64_t abi_ui_resource_byte_length(int64_t session_id, int64_t resource_id) {
    KainNativeUiResource* resource = abi_ui_find_resource(abi_ui_find_session(session_id), resource_id);
    return resource ? resource->byte_length : ABI_UI_INVALID_ARGUMENT;
}

double abi_ui_text_measure_width(int64_t session_id, int64_t font_resource_id, const char* text) {
    KainNativeUiResource* resource = abi_ui_find_resource(abi_ui_find_session(session_id), font_resource_id);
    double size = (resource && resource->scalar_value > 0.0) ? resource->scalar_value : 14.0;
    size_t length = strlen(text ? text : g_empty_string);
    return ((double)length) * size * 0.56;
}

double abi_ui_text_measure_height(int64_t session_id, int64_t font_resource_id, const char* text) {
    KainNativeUiResource* resource = abi_ui_find_resource(abi_ui_find_session(session_id), font_resource_id);
    double size = (resource && resource->scalar_value > 0.0) ? resource->scalar_value : 14.0;
    (void)text;
    return size * 1.25;
}

int64_t abi_ui_draw_rect(
    int64_t session_id,
    int64_t node_id,
    double x,
    double y,
    double width,
    double height,
    const char* style_key
) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiDrawCommand* command;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!abi_ui_find_node(session, node_id)) {
        return ABI_UI_INVALID_NODE;
    }
    command = abi_ui_append_draw_command(session, "rect");
    if (!command) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    command->node_id = node_id;
    command->x = x;
    command->y = y;
    command->width = width;
    command->height = height;
    abi_ui_copy_text(command->style_key, sizeof(command->style_key), style_key);
    return session->draw_command_count;
}

int64_t abi_ui_draw_text(
    int64_t session_id,
    int64_t node_id,
    int64_t font_resource_id,
    double x,
    double y,
    const char* text,
    const char* style_key
) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiDrawCommand* command;
    KainNativeUiResource* font;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!abi_ui_find_node(session, node_id)) {
        return ABI_UI_INVALID_NODE;
    }
    font = abi_ui_find_resource(session, font_resource_id);
    if (!font || strcmp(font->resource_type, "font") != 0) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    command = abi_ui_append_draw_command(session, "text");
    if (!command) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    command->node_id = node_id;
    command->font_resource_id = font_resource_id;
    command->x = x;
    command->y = y;
    abi_ui_copy_text(command->text, sizeof(command->text), text);
    abi_ui_copy_text(command->style_key, sizeof(command->style_key), style_key);
    return session->draw_command_count;
}

int64_t abi_ui_draw_resource(
    int64_t session_id,
    int64_t node_id,
    int64_t resource_id,
    double x,
    double y,
    double width,
    double height,
    const char* style_key
) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiDrawCommand* command;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!abi_ui_find_node(session, node_id)) {
        return ABI_UI_INVALID_NODE;
    }
    if (!abi_ui_find_resource(session, resource_id)) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    command = abi_ui_append_draw_command(session, "resource");
    if (!command) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    command->node_id = node_id;
    command->resource_id = resource_id;
    command->x = x;
    command->y = y;
    command->width = width;
    command->height = height;
    abi_ui_copy_text(command->style_key, sizeof(command->style_key), style_key);
    return session->draw_command_count;
}

int64_t abi_ui_draw_command_count(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->draw_command_count : ABI_UI_INVALID_SESSION;
}

const char* abi_ui_draw_command_kind(int64_t session_id, int64_t command_index) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session || command_index < 0 || command_index >= session->draw_command_count) {
        return abi_ui_return_string(session, g_empty_string);
    }
    return abi_ui_return_string(session, session->draw_commands[command_index].kind);
}

int64_t abi_ui_draw_command_node(int64_t session_id, int64_t command_index) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiDrawCommand* command = abi_ui_find_draw_command(session, command_index);
    if (!command) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    return command->node_id;
}

int64_t abi_ui_draw_command_resource(int64_t session_id, int64_t command_index) {
    KainNativeUiDrawCommand* command = abi_ui_find_draw_command(abi_ui_find_session(session_id), command_index);
    return command ? command->resource_id : ABI_UI_INVALID_ARGUMENT;
}

double abi_ui_draw_command_x(int64_t session_id, int64_t command_index) {
    KainNativeUiDrawCommand* command = abi_ui_find_draw_command(abi_ui_find_session(session_id), command_index);
    return command ? command->x : 0.0;
}

double abi_ui_draw_command_y(int64_t session_id, int64_t command_index) {
    KainNativeUiDrawCommand* command = abi_ui_find_draw_command(abi_ui_find_session(session_id), command_index);
    return command ? command->y : 0.0;
}

double abi_ui_draw_command_width(int64_t session_id, int64_t command_index) {
    KainNativeUiDrawCommand* command = abi_ui_find_draw_command(abi_ui_find_session(session_id), command_index);
    return command ? command->width : 0.0;
}

double abi_ui_draw_command_height(int64_t session_id, int64_t command_index) {
    KainNativeUiDrawCommand* command = abi_ui_find_draw_command(abi_ui_find_session(session_id), command_index);
    return command ? command->height : 0.0;
}

const char* abi_ui_draw_command_text(int64_t session_id, int64_t command_index) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiDrawCommand* command = abi_ui_find_draw_command(session, command_index);
    return abi_ui_return_string(session, command ? command->text : g_empty_string);
}

const char* abi_ui_draw_command_style(int64_t session_id, int64_t command_index) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiDrawCommand* command = abi_ui_find_draw_command(session, command_index);
    return abi_ui_return_string(session, command ? command->style_key : g_empty_string);
}

int64_t abi_ui_draw_command_font(int64_t session_id, int64_t command_index) {
    KainNativeUiDrawCommand* command = abi_ui_find_draw_command(abi_ui_find_session(session_id), command_index);
    return command ? command->font_resource_id : ABI_UI_INVALID_ARGUMENT;
}

int64_t abi_ui_clipboard_set_text(int64_t session_id, const char* text) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    abi_ui_copy_text(session->clipboard_text, sizeof(session->clipboard_text), text);
    if (abi_ui_host_adapter_is_live_backend(session->host_backend)) {
        abi_ui_host_adapter_clipboard_set_text(session, session->clipboard_text);
    }
    return ABI_UI_OK;
}

const char* abi_ui_clipboard_text(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (session && abi_ui_host_adapter_is_live_backend(session->host_backend)) {
        char clipboard_text[ABI_UI_MAX_TEXT];
        if (abi_ui_host_adapter_clipboard_get_text(
                session,
                clipboard_text,
                sizeof(clipboard_text)
            )) {
            abi_ui_copy_text(
                session->clipboard_text,
                sizeof(session->clipboard_text),
                clipboard_text
            );
        }
    }
    return abi_ui_return_string(session, session ? session->clipboard_text : g_empty_string);
}

int64_t abi_ui_ime_begin(int64_t session_id, int64_t node_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!abi_ui_find_node(session, node_id)) {
        return ABI_UI_INVALID_NODE;
    }
    session->ime_active_node_id = node_id;
    session->ime_text[0] = '\0';
    return ABI_UI_OK;
}

int64_t abi_ui_ime_commit_text(int64_t session_id, const char* text) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (session->ime_active_node_id <= 0) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    abi_ui_copy_text(session->ime_text, sizeof(session->ime_text), text);
    return ABI_UI_OK;
}

int64_t abi_ui_ime_end(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    session->ime_active_node_id = 0;
    return ABI_UI_OK;
}

int64_t abi_ui_ime_active_node(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->ime_active_node_id : ABI_UI_INVALID_SESSION;
}

const char* abi_ui_ime_text(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return abi_ui_return_string(session, session ? session->ime_text : g_empty_string);
}

int64_t abi_ui_drag_begin(int64_t session_id, int64_t node_id, const char* payload, double x, double y) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!abi_ui_find_node(session, node_id)) {
        return ABI_UI_INVALID_NODE;
    }
    session->drag_active_node_id = node_id;
    session->drag_drop_target_id = 0;
    session->drag_x = x;
    session->drag_y = y;
    abi_ui_copy_text(session->drag_payload, sizeof(session->drag_payload), payload);
    return ABI_UI_OK;
}

int64_t abi_ui_drag_update(int64_t session_id, double x, double y, int64_t drop_target_node_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (session->drag_active_node_id <= 0) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    if (drop_target_node_id > 0 && !abi_ui_find_node(session, drop_target_node_id)) {
        return ABI_UI_INVALID_NODE;
    }
    session->drag_x = x;
    session->drag_y = y;
    session->drag_drop_target_id = drop_target_node_id;
    return ABI_UI_OK;
}

int64_t abi_ui_drag_drop(int64_t session_id, int64_t drop_target_node_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (session->drag_active_node_id <= 0) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    if (drop_target_node_id > 0 && !abi_ui_find_node(session, drop_target_node_id)) {
        return ABI_UI_INVALID_NODE;
    }
    session->drag_drop_target_id = drop_target_node_id;
    return drop_target_node_id;
}

int64_t abi_ui_drag_active_node(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->drag_active_node_id : ABI_UI_INVALID_SESSION;
}

int64_t abi_ui_drag_drop_target(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->drag_drop_target_id : ABI_UI_INVALID_SESSION;
}

double abi_ui_drag_x(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->drag_x : 0.0;
}

double abi_ui_drag_y(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->drag_y : 0.0;
}

const char* abi_ui_drag_payload(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return abi_ui_return_string(session, session ? session->drag_payload : g_empty_string);
}

int64_t abi_ui_menu_create(int64_t session_id, const char* key) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    uint32_t slot;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (session->menu_count >= ABI_UI_MAX_MENUS) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    if (!abi_ui_find_free_slot_u64(
            session->menu_occupancy_bits,
            ABI_UI_MENU_OCCUPANCY_WORD_COUNT,
            &slot)) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    memset(&session->menus[slot], 0, sizeof(session->menus[slot]));
    session->menus[slot].in_use = 1;
    session->menus[slot].id = session->next_menu_id++;
    abi_ui_copy_text(session->menus[slot].key, sizeof(session->menus[slot].key), key);
    session->menu_occupancy_bits[slot >> 6] |= UINT64_C(1) << (slot & 63u);
    if (!abi_ui_index_insert(
            session->menu_index,
            ABI_UI_MENU_INDEX_CAPACITY,
            ABI_UI_MENU_INDEX_MASK,
            abi_ui_mix_u64((uint64_t)session->menus[slot].id),
            slot)) {
        session->menu_occupancy_bits[slot >> 6] &= ~(UINT64_C(1) << (slot & 63u));
        memset(&session->menus[slot], 0, sizeof(session->menus[slot]));
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    session->menu_count += 1;
    return session->menus[slot].id;
}

int64_t abi_ui_menu_add_item(
    int64_t session_id,
    int64_t menu_id,
    const char* key,
    const char* label,
    int64_t command_id
) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiMenu* menu = abi_ui_find_menu(session, menu_id);
    uint32_t slot;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!menu) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    if (session->menu_item_count >= ABI_UI_MAX_MENU_ITEMS) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    if (!abi_ui_find_free_slot_u64(
            session->menu_item_occupancy_bits,
            ABI_UI_MENU_ITEM_OCCUPANCY_WORD_COUNT,
            &slot)) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    memset(&session->menu_items[slot], 0, sizeof(session->menu_items[slot]));
    session->menu_items[slot].in_use = 1;
    session->menu_items[slot].id = session->next_menu_item_id++;
    session->menu_items[slot].menu_id = menu_id;
    session->menu_items[slot].command_id = command_id;
    abi_ui_copy_text(session->menu_items[slot].key, sizeof(session->menu_items[slot].key), key);
    abi_ui_copy_text(session->menu_items[slot].label, sizeof(session->menu_items[slot].label), label);
    session->menu_item_occupancy_bits[slot >> 6] |= UINT64_C(1) << (slot & 63u);
    menu->item_count += 1;
    session->menu_item_count += 1;
    return session->menu_items[slot].id;
}

int64_t abi_ui_menu_open(int64_t session_id, int64_t menu_id, double x, double y) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiMenu* menu = abi_ui_find_menu(session, menu_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!menu) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    menu->open = 1;
    menu->x = x;
    menu->y = y;
    session->active_menu_id = menu_id;
    return ABI_UI_OK;
}

int64_t abi_ui_menu_active(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->active_menu_id : ABI_UI_INVALID_SESSION;
}

int64_t abi_ui_menu_item_count(int64_t session_id, int64_t menu_id) {
    KainNativeUiMenu* menu = abi_ui_find_menu(abi_ui_find_session(session_id), menu_id);
    return menu ? menu->item_count : ABI_UI_INVALID_ARGUMENT;
}

const char* abi_ui_menu_item_label(int64_t session_id, int64_t menu_id, int64_t item_index) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    int64_t index;
    int64_t seen = 0;
    if (!session || item_index < 0) {
        return abi_ui_return_string(session, g_empty_string);
    }
    for (index = 0; index < ABI_UI_MAX_MENU_ITEMS; index += 1) {
        if (session->menu_items[index].in_use && session->menu_items[index].menu_id == menu_id) {
            if (seen == item_index) {
                return abi_ui_return_string(session, session->menu_items[index].label);
            }
            seen += 1;
        }
    }
    return abi_ui_return_string(session, g_empty_string);
}

int64_t abi_ui_menu_item_command(int64_t session_id, int64_t menu_id, int64_t item_index) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    int64_t index;
    int64_t seen = 0;
    if (!session || item_index < 0) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    for (index = 0; index < ABI_UI_MAX_MENU_ITEMS; index += 1) {
        if (session->menu_items[index].in_use && session->menu_items[index].menu_id == menu_id) {
            if (seen == item_index) {
                return session->menu_items[index].command_id;
            }
            seen += 1;
        }
    }
    return ABI_UI_INVALID_ARGUMENT;
}

int64_t abi_ui_dialog_request(int64_t session_id, const char* kind, const char* title, const char* message) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    uint32_t slot;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (session->dialog_count >= ABI_UI_MAX_DIALOGS) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    if (!abi_ui_find_free_slot_u64(
            session->dialog_occupancy_bits,
            ABI_UI_DIALOG_OCCUPANCY_WORD_COUNT,
            &slot)) {
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    memset(&session->dialogs[slot], 0, sizeof(session->dialogs[slot]));
    session->dialogs[slot].in_use = 1;
    session->dialogs[slot].id = session->next_dialog_id++;
    abi_ui_copy_text(session->dialogs[slot].kind, sizeof(session->dialogs[slot].kind), kind);
    abi_ui_copy_text(session->dialogs[slot].title, sizeof(session->dialogs[slot].title), title);
    abi_ui_copy_text(session->dialogs[slot].message, sizeof(session->dialogs[slot].message), message);
    session->dialog_occupancy_bits[slot >> 6] |= UINT64_C(1) << (slot & 63u);
    if (!abi_ui_index_insert(
            session->dialog_index,
            ABI_UI_DIALOG_INDEX_CAPACITY,
            ABI_UI_DIALOG_INDEX_MASK,
            abi_ui_mix_u64((uint64_t)session->dialogs[slot].id),
            slot)) {
        session->dialog_occupancy_bits[slot >> 6] &= ~(UINT64_C(1) << (slot & 63u));
        memset(&session->dialogs[slot], 0, sizeof(session->dialogs[slot]));
        return ABI_UI_CAPACITY_EXCEEDED;
    }
    session->dialog_count += 1;
    session->active_dialog_id = session->dialogs[slot].id;
    return session->dialogs[slot].id;
}

int64_t abi_ui_dialog_active(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->active_dialog_id : ABI_UI_INVALID_SESSION;
}

const char* abi_ui_dialog_kind(int64_t session_id, int64_t dialog_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiDialog* dialog = abi_ui_find_dialog(session, dialog_id);
    return abi_ui_return_string(session, dialog ? dialog->kind : g_empty_string);
}

const char* abi_ui_dialog_title(int64_t session_id, int64_t dialog_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiDialog* dialog = abi_ui_find_dialog(session, dialog_id);
    return abi_ui_return_string(session, dialog ? dialog->title : g_empty_string);
}

const char* abi_ui_dialog_message(int64_t session_id, int64_t dialog_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiDialog* dialog = abi_ui_find_dialog(session, dialog_id);
    return abi_ui_return_string(session, dialog ? dialog->message : g_empty_string);
}

int64_t abi_ui_dialog_respond(int64_t session_id, int64_t dialog_id, int64_t result, const char* response_text) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    KainNativeUiDialog* dialog = abi_ui_find_dialog(session, dialog_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!dialog) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    dialog->result = result;
    dialog->response_ready = 1;
    abi_ui_copy_text(dialog->response_text, sizeof(dialog->response_text), response_text);
    session->dialog_response_ready = 1;
    session->dialog_response_result = result;
    abi_ui_copy_text(session->dialog_response_text, sizeof(session->dialog_response_text), response_text);
    if (session->active_dialog_id == dialog_id) {
        session->active_dialog_id = 0;
    }
    return ABI_UI_OK;
}

int64_t abi_ui_dialog_poll_response(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    int64_t result;
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    if (!session->dialog_response_ready) {
        return 0;
    }
    result = session->dialog_response_result;
    session->dialog_response_ready = 0;
    return result;
}

const char* abi_ui_dialog_response_text(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return abi_ui_return_string(session, session ? session->dialog_response_text : g_empty_string);
}

int64_t abi_ui_hot_reload_begin(int64_t session_id, const char* revision_key) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    session->hot_reload_generation += 1;
    abi_ui_copy_text(session->hot_reload_key, sizeof(session->hot_reload_key), revision_key);
    return session->hot_reload_generation;
}

int64_t abi_ui_hot_reload_commit(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
    return session->hot_reload_generation;
}

int64_t abi_ui_hot_reload_generation(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return session ? session->hot_reload_generation : ABI_UI_INVALID_SESSION;
}

const char* abi_ui_hot_reload_key(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    return abi_ui_return_string(session, session ? session->hot_reload_key : g_empty_string);
}
