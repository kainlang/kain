#include "../../include/kain_runtime_ui.h"

#ifdef _WIN32
static const char* kain_ui_find_substring(
    const char* start,
    const char* end,
    const char* needle
) {
    size_t needle_len;
    const char* cursor;

    if (!start || !end || !needle) {
        return NULL;
    }

    needle_len = strlen(needle);
    if (needle_len == 0 || start >= end || (size_t)(end - start) < needle_len) {
        return NULL;
    }

    for (cursor = start; cursor + needle_len <= end; ++cursor) {
        if (memcmp(cursor, needle, needle_len) == 0) {
            return cursor;
        }
    }
    return NULL;
}

static const char* kain_ui_skip_ws(const char* cursor, const char* end) {
    while (cursor && cursor < end && (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' || *cursor == '\t')) {
        cursor += 1;
    }
    return cursor;
}

static const char* kain_ui_find_matching(
    const char* start,
    const char* end,
    char open_ch,
    char close_ch
) {
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    const char* cursor;

    if (!start || start >= end || *start != open_ch) {
        return NULL;
    }

    for (cursor = start; cursor < end; ++cursor) {
        char ch = *cursor;
        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (ch == '\\') {
                escaped = 1;
            } else if (ch == '"') {
                in_string = 0;
            }
            continue;
        }

        if (ch == '"') {
            in_string = 1;
            continue;
        }

        if (ch == open_ch) {
            depth += 1;
        } else if (ch == close_ch) {
            depth -= 1;
            if (depth == 0) {
                return cursor;
            }
        }
    }

    return NULL;
}

static const char* kain_ui_find_value_start(
    const char* scope_start,
    const char* scope_end,
    const char* key
) {
    const char* key_pos = kain_ui_find_substring(scope_start, scope_end, key);
    const char* colon;

    if (!key_pos) {
        return NULL;
    }

    colon = key_pos + strlen(key);
    while (colon < scope_end && *colon != ':') {
        colon += 1;
    }
    if (colon >= scope_end || *colon != ':') {
        return NULL;
    }

    return kain_ui_skip_ws(colon + 1, scope_end);
}

static void kain_ui_copy_string_value(
    const char* value_start,
    const char* scope_end,
    char* out,
    size_t out_cap
) {
    const char* cursor;
    size_t written = 0;

    if (!out || out_cap == 0) {
        return;
    }
    out[0] = '\0';
    if (!value_start || value_start >= scope_end) {
        return;
    }

    if (value_start + 4 <= scope_end && memcmp(value_start, "null", 4) == 0) {
        return;
    }
    if (*value_start != '"') {
        return;
    }

    cursor = value_start + 1;
    while (cursor < scope_end && *cursor != '"' && written + 1 < out_cap) {
        char ch = *cursor++;
        if (ch == '\\' && cursor < scope_end) {
            char escaped = *cursor++;
            switch (escaped) {
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                case '\\': ch = '\\'; break;
                case '"': ch = '"'; break;
                default: ch = escaped; break;
            }
        }
        out[written++] = ch;
    }
    out[written] = '\0';
}

static void kain_ui_extract_string_field(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    char* out,
    size_t out_cap
) {
    const char* value_start = kain_ui_find_value_start(scope_start, scope_end, key);
    kain_ui_copy_string_value(value_start, scope_end, out, out_cap);
}

static int kain_ui_extract_u64_field(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    unsigned long long* out_value
) {
    const char* value_start = kain_ui_find_value_start(scope_start, scope_end, key);
    char* end_ptr = NULL;
    unsigned long long value;

    if (!out_value || !value_start || value_start >= scope_end || *value_start == '"') {
        return 0;
    }
    if (value_start + 4 <= scope_end && memcmp(value_start, "null", 4) == 0) {
        return 0;
    }

    value = _strtoui64(value_start, &end_ptr, 10);
    if (!end_ptr || end_ptr == value_start) {
        return 0;
    }

    *out_value = value;
    return 1;
}

static int kain_ui_extract_i32_field(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    int* out_value
) {
    const char* value_start = kain_ui_find_value_start(scope_start, scope_end, key);
    char* end_ptr = NULL;
    long value;

    if (!out_value || !value_start || value_start >= scope_end || *value_start == '\"') {
        return 0;
    }
    if (value_start + 4 <= scope_end && memcmp(value_start, "null", 4) == 0) {
        return 0;
    }

    value = strtol(value_start, &end_ptr, 10);
    if (!end_ptr || end_ptr == value_start) {
        return 0;
    }

    *out_value = (int)value;
    return 1;
}

static int kain_ui_extract_f32_field(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    float* out_value
) {
    const char* value_start = kain_ui_find_value_start(scope_start, scope_end, key);
    char* end_ptr = NULL;
    double value;

    if (!out_value || !value_start || value_start >= scope_end || *value_start == '\"') {
        return 0;
    }
    if (value_start + 4 <= scope_end && memcmp(value_start, "null", 4) == 0) {
        return 0;
    }

    value = strtod(value_start, &end_ptr);
    if (!end_ptr || end_ptr == value_start) {
        return 0;
    }

    *out_value = (float)value;
    return 1;
}

static int kain_ui_extract_bool_field(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    int* out_value
) {
    const char* value_start = kain_ui_find_value_start(scope_start, scope_end, key);

    if (!out_value || !value_start || value_start >= scope_end) {
        return 0;
    }
    if (value_start + 4 <= scope_end && memcmp(value_start, "true", 4) == 0) {
        *out_value = 1;
        return 1;
    }
    if (value_start + 5 <= scope_end && memcmp(value_start, "false", 5) == 0) {
        *out_value = 0;
        return 1;
    }

    return 0;
}

static KainUiCompiledNodeKind kain_ui_parse_node_kind(const char* value) {
    if (!value || !value[0]) return KAIN_UI_COMPILED_NODE_UNKNOWN;
    if (_stricmp(value, "Element") == 0) return KAIN_UI_COMPILED_NODE_ELEMENT;
    if (_stricmp(value, "ComponentRef") == 0) return KAIN_UI_COMPILED_NODE_COMPONENT_REF;
    if (_stricmp(value, "Text") == 0) return KAIN_UI_COMPILED_NODE_TEXT;
    if (_stricmp(value, "Panel") == 0) return KAIN_UI_COMPILED_NODE_PANEL;
    if (_stricmp(value, "Inspector") == 0) return KAIN_UI_COMPILED_NODE_INSPECTOR;
    if (_stricmp(value, "Graph") == 0) return KAIN_UI_COMPILED_NODE_GRAPH;
    if (_stricmp(value, "Timeline") == 0) return KAIN_UI_COMPILED_NODE_TIMELINE;
    if (_stricmp(value, "Table") == 0) return KAIN_UI_COMPILED_NODE_TABLE;
    if (_stricmp(value, "Tree") == 0) return KAIN_UI_COMPILED_NODE_TREE;
    if (_stricmp(value, "Viewport2D") == 0) return KAIN_UI_COMPILED_NODE_VIEWPORT2D;
    if (_stricmp(value, "Viewport3D") == 0) return KAIN_UI_COMPILED_NODE_VIEWPORT3D;
    if (_stricmp(value, "Overlay") == 0) return KAIN_UI_COMPILED_NODE_OVERLAY;
    if (_stricmp(value, "Slot") == 0) return KAIN_UI_COMPILED_NODE_SLOT;
    return KAIN_UI_COMPILED_NODE_UNKNOWN;
}

static int kain_ui_parse_projection_node(
    const char* object_start,
    const char* object_end,
    KainUiCompiledNode* node
) {
    char kind_value[64];
    unsigned long long value = 0;

    if (!object_start || !object_end || !node) {
        return 0;
    }

    ZeroMemory(node, sizeof(*node));
    if (!kain_ui_extract_u64_field(object_start, object_end, "\"id\"", &node->id)) {
        return 0;
    }
    if (kain_ui_extract_u64_field(object_start, object_end, "\"parent_id\"", &value)) {
        node->parent_id = value;
        node->has_parent = 1;
    }
    if (kain_ui_extract_u64_field(object_start, object_end, "\"depth\"", &value)) {
        node->depth = (unsigned int)value;
    }
    if (kain_ui_extract_u64_field(object_start, object_end, "\"child_count\"", &value)) {
        node->child_count = (int)value;
    }
    kind_value[0] = '\0';
    kain_ui_extract_string_field(object_start, object_end, "\"kind\"", kind_value, sizeof(kind_value));
    node->kind = kain_ui_parse_node_kind(kind_value);
    kain_ui_extract_string_field(object_start, object_end, "\"title\"", node->title, sizeof(node->title));
    kain_ui_extract_string_field(object_start, object_end, "\"text\"", node->text, sizeof(node->text));
    kain_ui_extract_string_field(object_start, object_end, "\"tag\"", node->tag, sizeof(node->tag));
    kain_ui_extract_string_field(object_start, object_end, "\"scene\"", node->scene, sizeof(node->scene));
    kain_ui_extract_string_field(
        object_start,
        object_end,
        "\"layout_kind\"",
        node->layout_kind,
        sizeof(node->layout_kind)
    );
    kain_ui_extract_string_field(
        object_start,
        object_end,
        "\"dock_placement\"",
        node->dock_placement,
        sizeof(node->dock_placement)
    );
    if (kain_ui_extract_f32_field(object_start, object_end, "\"split_ratio\"", &node->split_ratio)) {
        node->has_split_ratio = 1;
    }
    kain_ui_extract_bool_field(object_start, object_end, "\"resizable\"", &node->resizable);
    kain_ui_extract_string_field(
        object_start,
        object_end,
        "\"persistent_layout_id\"",
        node->persistent_layout_id,
        sizeof(node->persistent_layout_id)
    );
    kain_ui_extract_string_field(
        object_start,
        object_end,
        "\"tab_group_id\"",
        node->tab_group_id,
        sizeof(node->tab_group_id)
    );
    kain_ui_extract_string_field(
        object_start,
        object_end,
        "\"tab_label\"",
        node->tab_label,
        sizeof(node->tab_label)
    );
    if (kain_ui_extract_i32_field(object_start, object_end, "\"tab_order\"", &node->tab_order)) {
        node->has_tab_order = 1;
    }
    kain_ui_extract_bool_field(
        object_start,
        object_end,
        "\"tab_default_active\"",
        &node->tab_default_active
    );
    kain_ui_extract_bool_field(object_start, object_end, "\"tab_closable\"", &node->tab_closable);
    kain_ui_extract_bool_field(object_start, object_end, "\"tab_is_active\"", &node->tab_is_active);
    return 1;
}

static int kain_ui_parse_projection_tab_group(
    const char* object_start,
    const char* object_end,
    KainUiCompiledTabGroup* tab_group
) {
    if (!object_start || !object_end || !tab_group) {
        return 0;
    }

    ZeroMemory(tab_group, sizeof(*tab_group));
    kain_ui_extract_string_field(object_start, object_end, "\"id\"", tab_group->id, sizeof(tab_group->id));
    if (!tab_group->id[0]) {
        return 0;
    }
    kain_ui_extract_string_field(
        object_start,
        object_end,
        "\"active_tab_layout_id\"",
        tab_group->active_tab_layout_id,
        sizeof(tab_group->active_tab_layout_id)
    );
    kain_ui_extract_i32_field(object_start, object_end, "\"tab_count\"", &tab_group->tab_count);
    return 1;
}

void kain_ui_compiled_bundle_init(KainUiCompiledBundle* bundle) {
    if (!bundle) {
        return;
    }
    ZeroMemory(bundle, sizeof(*bundle));
}

int kain_ui_compiled_bundle_load_from_json(const char* json, KainUiCompiledBundle* bundle) {
    const char* json_end;
    const char* projection_key;
    const char* projection_start;
    const char* projection_end;
    const char* nodes_key;
    const char* nodes_array_start;
    const char* nodes_array_end;
    const char* cursor;

    if (!json || !bundle) {
        return 0;
    }

    kain_ui_compiled_bundle_init(bundle);
    json_end = json + strlen(json);

    kain_ui_extract_string_field(json, json_end, "\"window_title\"", bundle->window_title, sizeof(bundle->window_title));

    projection_key = kain_ui_find_substring(json, json_end, "\"native_projection\"");
    if (!projection_key) {
        return 0;
    }

    projection_start = kain_ui_find_value_start(projection_key, json_end, "\"native_projection\"");
    if (!projection_start || projection_start >= json_end || *projection_start != '{') {
        return 0;
    }
    projection_end = kain_ui_find_matching(projection_start, json_end, '{', '}');
    if (!projection_end) {
        return 0;
    }

    if (kain_ui_extract_u64_field(projection_start, projection_end, "\"root_id\"", &bundle->root_id)) {
        bundle->has_root_id = 1;
    }
    kain_ui_extract_string_field(
        projection_start,
        projection_end,
        "\"primary_panel_title\"",
        bundle->primary_panel_title,
        sizeof(bundle->primary_panel_title)
    );
    kain_ui_extract_string_field(
        projection_start,
        projection_end,
        "\"primary_viewport_title\"",
        bundle->primary_viewport_title,
        sizeof(bundle->primary_viewport_title)
    );
    kain_ui_extract_string_field(
        projection_start,
        projection_end,
        "\"primary_viewport_scene\"",
        bundle->primary_viewport_scene,
        sizeof(bundle->primary_viewport_scene)
    );

    nodes_key = kain_ui_find_substring(projection_start, projection_end, "\"nodes\"");
    if (!nodes_key) {
        bundle->loaded = 1;
        return 1;
    }

    nodes_array_start = kain_ui_find_value_start(nodes_key, projection_end, "\"nodes\"");
    if (!nodes_array_start || nodes_array_start >= projection_end || *nodes_array_start != '[') {
        bundle->loaded = 1;
        return 1;
    }
    nodes_array_end = kain_ui_find_matching(nodes_array_start, projection_end + 1, '[', ']');
    if (!nodes_array_end) {
        return 0;
    }

    cursor = nodes_array_start + 1;
    while (cursor < nodes_array_end && bundle->node_count < KAIN_UI_COMPILED_BUNDLE_MAX_NODES) {
        const char* object_start = strchr(cursor, '{');
        const char* object_end;
        if (!object_start || object_start >= nodes_array_end) {
            break;
        }
        object_end = kain_ui_find_matching(object_start, nodes_array_end + 1, '{', '}');
        if (!object_end) {
            break;
        }
        if (kain_ui_parse_projection_node(object_start, object_end, &bundle->nodes[bundle->node_count])) {
            bundle->node_count += 1;
        }
        cursor = object_end + 1;
    }

    bundle->loaded = 1;
    return 1;
}

int kain_ui_compiled_bundle_load_from_path(const char* path, KainUiCompiledBundle* bundle) {
    FILE* file = NULL;
    long file_size;
    char* contents;
    size_t read_count;
    int result;

    if (!path || !path[0] || !bundle) {
        return 0;
    }

    if (fopen_s(&file, path, "rb") != 0 || !file) {
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    file_size = ftell(file);
    if (file_size <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    contents = (char*)malloc((size_t)file_size + 1);
    if (!contents) {
        fclose(file);
        return 0;
    }

    read_count = fread(contents, 1, (size_t)file_size, file);
    fclose(file);
    contents[read_count] = '\0';
    result = kain_ui_compiled_bundle_load_from_json(contents, bundle);
    free(contents);
    return result;
}

int kain_ui_compiled_bundle_load_from_env(const char* env_name, KainUiCompiledBundle* bundle) {
    char* path;
    int result;

    if (!env_name || !bundle) {
        return 0;
    }

    path = kain_env_dup(env_name);
    if (!path) {
        return 0;
    }

    result = kain_ui_compiled_bundle_load_from_path(path, bundle);
    kain_env_free(path);
    return result;
}

const KainUiCompiledNode* kain_ui_compiled_bundle_find_first_kind(
    const KainUiCompiledBundle* bundle,
    KainUiCompiledNodeKind kind
) {
    int index;

    if (!bundle || !bundle->loaded) {
        return NULL;
    }

    for (index = 0; index < bundle->node_count; ++index) {
        if (bundle->nodes[index].kind == kind) {
            return &bundle->nodes[index];
        }
    }

    return NULL;
}
#endif
