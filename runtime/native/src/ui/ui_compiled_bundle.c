#include "../../include/ui_bundle.h"

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
    if (value_start && value_start < scope_end && *value_start == '{') {
        const char* value_end = kain_ui_find_matching(value_start, scope_end, '{', '}');
        if (value_end) {
            const char* tagged_string_value = kain_ui_find_value_start(value_start, value_end, "\"String\"");
            if (tagged_string_value) {
                kain_ui_copy_string_value(tagged_string_value, value_end, out, out_cap);
                return;
            }
        }
    }
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

static KainUiCompiledNodeKind kain_ui_parse_node_kind(const char* value);

static int kain_ui_extract_object_field_bounds(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    const char** out_start,
    const char** out_end
) {
    const char* value_start;
    const char* value_end;

    if (!out_start || !out_end) {
        return 0;
    }
    *out_start = NULL;
    *out_end = NULL;

    value_start = kain_ui_find_value_start(scope_start, scope_end, key);
    if (!value_start || value_start >= scope_end || *value_start != '{') {
        return 0;
    }
    value_end = kain_ui_find_matching(value_start, scope_end + 1, '{', '}');
    if (!value_end) {
        return 0;
    }

    *out_start = value_start;
    *out_end = value_end;
    return 1;
}

static int kain_ui_extract_array_field_bounds(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    const char** out_start,
    const char** out_end
) {
    const char* value_start;
    const char* value_end;

    if (!out_start || !out_end) {
        return 0;
    }
    *out_start = NULL;
    *out_end = NULL;

    value_start = kain_ui_find_value_start(scope_start, scope_end, key);
    if (!value_start || value_start >= scope_end || *value_start != '[') {
        return 0;
    }
    value_end = kain_ui_find_matching(value_start, scope_end + 1, '[', ']');
    if (!value_end) {
        return 0;
    }

    *out_start = value_start;
    *out_end = value_end;
    return 1;
}

static int kain_ui_parse_kind_value(
    const char* scope_start,
    const char* scope_end,
    const char* key,
    KainUiCompiledNodeKind* out_kind,
    char* out_tag,
    size_t out_tag_cap
) {
    const char* value_start;

    if (!out_kind) {
        return 0;
    }

    value_start = kain_ui_find_value_start(scope_start, scope_end, key);
    if (!value_start || value_start >= scope_end) {
        return 0;
    }

    if (*value_start == '"') {
        char kind_value[64];
        kain_ui_copy_string_value(value_start, scope_end, kind_value, sizeof(kind_value));
        *out_kind = kain_ui_parse_node_kind(kind_value);
        return 1;
    }

    if (*value_start == '{') {
        const char* value_end = kain_ui_find_matching(value_start, scope_end + 1, '{', '}');
        if (!value_end) {
            return 0;
        }

        if (kain_ui_find_substring(value_start, value_end, "\"Element\"")) {
            *out_kind = KAIN_UI_COMPILED_NODE_ELEMENT;
            if (out_tag && out_tag_cap > 0) {
                kain_ui_extract_string_field(value_start, value_end, "\"Element\"", out_tag, out_tag_cap);
            }
            return 1;
        }
        if (kain_ui_find_substring(value_start, value_end, "\"ComponentRef\"")) {
            *out_kind = KAIN_UI_COMPILED_NODE_COMPONENT_REF;
            if (out_tag && out_tag_cap > 0) {
                kain_ui_extract_string_field(value_start, value_end, "\"ComponentRef\"", out_tag, out_tag_cap);
            }
            return 1;
        }
    }

    return 0;
}

static void kain_ui_default_tag_for_kind(
    KainUiCompiledNodeKind kind,
    char* out_tag,
    size_t out_tag_cap
) {
    const char* value = "";

    if (!out_tag || out_tag_cap == 0 || out_tag[0]) {
        return;
    }

    /* LUT replaces switch — proven equivalent for sequential enum values 0..13 */
    {
        static const char* const KIND_TAGS[] = {
            "",          /* UNKNOWN=0 */
            "element",   /* ELEMENT=1 */
            "component", /* COMPONENT_REF=2 */
            "text",      /* TEXT=3 */
            "panel",     /* PANEL=4 */
            "inspector", /* INSPECTOR=5 */
            "graph",     /* GRAPH=6 */
            "timeline",  /* TIMELINE=7 */
            "table",     /* TABLE=8 */
            "tree",      /* TREE=9 */
            "viewport2d",/* VIEWPORT2D=10 */
            "viewport3d",/* VIEWPORT3D=11 */
            "overlay",   /* OVERLAY=12 */
            "slot",      /* SLOT=13 */
        };
        unsigned int idx = (unsigned int)kind;
        value = (idx < sizeof(KIND_TAGS)/sizeof(KIND_TAGS[0])) ? KIND_TAGS[idx] : "";
    }

    if (value[0]) {
        strncpy_s(out_tag, out_tag_cap, value, _TRUNCATE);
    }
}

static int kain_ui_find_node_index_by_id(
    const KainUiCompiledBundle* bundle,
    unsigned long long id
) {
    int index;

    if (!bundle) {
        return -1;
    }

    for (index = 0; index < bundle->node_count; ++index) {
        if (bundle->nodes[index].id == id) {
            return index;
        }
    }

    return -1;
}

static unsigned int kain_ui_compute_node_depth(
    const KainUiCompiledBundle* bundle,
    int node_index,
    int recursion_guard
) {
    const KainUiCompiledNode* node;
    int parent_index;

    if (!bundle || node_index < 0 || node_index >= bundle->node_count || recursion_guard > KAIN_UI_COMPILED_BUNDLE_MAX_NODES) {
        return 0u;
    }

    node = &bundle->nodes[node_index];
    if (!node->has_parent) {
        return 0u;
    }

    parent_index = kain_ui_find_node_index_by_id(bundle, node->parent_id);
    if (parent_index < 0) {
        return 0u;
    }

    return kain_ui_compute_node_depth(bundle, parent_index, recursion_guard + 1) + 1u;
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

static int kain_ui_parse_tree_node(
    const char* object_start,
    const char* object_end,
    KainUiCompiledNode* node
) {
    const char* layout_start = NULL;
    const char* layout_end = NULL;
    const char* props_start = NULL;
    const char* props_end = NULL;
    const char* children_start = NULL;
    const char* children_end = NULL;
    const char* cursor;
    char kind_tag[KAIN_UI_COMPILED_BUNDLE_MAX_TAG];
    unsigned long long value = 0;

    if (!object_start || !object_end || !node) {
        return 0;
    }

    ZeroMemory(node, sizeof(*node));
    if (!kain_ui_extract_u64_field(object_start, object_end, "\"id\"", &node->id)) {
        return 0;
    }

    ZeroMemory(kind_tag, sizeof(kind_tag));
    if (!kain_ui_parse_kind_value(object_start, object_end, "\"kind\"", &node->kind, kind_tag, sizeof(kind_tag))) {
        node->kind = KAIN_UI_COMPILED_NODE_UNKNOWN;
    }
    if (kind_tag[0]) {
        strncpy_s(node->tag, sizeof(node->tag), kind_tag, _TRUNCATE);
    }

    if (kain_ui_extract_object_field_bounds(object_start, object_end, "\"props\"", &props_start, &props_end)) {
        kain_ui_extract_string_field(props_start, props_end, "\"title\"", node->title, sizeof(node->title));
        kain_ui_extract_string_field(props_start, props_end, "\"text\"", node->text, sizeof(node->text));
        kain_ui_extract_string_field(props_start, props_end, "\"tag\"", node->tag, sizeof(node->tag));
        kain_ui_extract_string_field(props_start, props_end, "\"scene\"", node->scene, sizeof(node->scene));
    }

    if (kain_ui_extract_object_field_bounds(object_start, object_end, "\"layout\"", &layout_start, &layout_end)) {
        kain_ui_extract_string_field(layout_start, layout_end, "\"kind\"", node->layout_kind, sizeof(node->layout_kind));
        kain_ui_extract_string_field(layout_start, layout_end, "\"dock\"", node->dock_placement, sizeof(node->dock_placement));
        if (kain_ui_extract_f32_field(layout_start, layout_end, "\"split_ratio\"", &node->split_ratio)) {
            node->has_split_ratio = 1;
        }
        kain_ui_extract_bool_field(layout_start, layout_end, "\"resizable\"", &node->resizable);
        kain_ui_extract_string_field(
            layout_start,
            layout_end,
            "\"persistent_layout_id\"",
            node->persistent_layout_id,
            sizeof(node->persistent_layout_id)
        );
        kain_ui_extract_string_field(
            layout_start,
            layout_end,
            "\"tab_group_id\"",
            node->tab_group_id,
            sizeof(node->tab_group_id)
        );
        kain_ui_extract_string_field(
            layout_start,
            layout_end,
            "\"tab_label\"",
            node->tab_label,
            sizeof(node->tab_label)
        );
        if (kain_ui_extract_i32_field(layout_start, layout_end, "\"tab_order\"", &node->tab_order)) {
            node->has_tab_order = 1;
        }
        kain_ui_extract_bool_field(layout_start, layout_end, "\"tab_default_active\"", &node->tab_default_active);
        kain_ui_extract_bool_field(layout_start, layout_end, "\"tab_closable\"", &node->tab_closable);
    }

    if (kain_ui_extract_array_field_bounds(object_start, object_end, "\"children\"", &children_start, &children_end)) {
        cursor = children_start + 1;
        while (cursor < children_end) {
            char* end_ptr = NULL;
            value = _strtoui64(cursor, &end_ptr, 10);
            if (end_ptr && end_ptr > cursor) {
                node->child_count += 1;
                cursor = end_ptr;
                continue;
            }
            cursor += 1;
        }
    }

    kain_ui_default_tag_for_kind(node->kind, node->tag, sizeof(node->tag));
    return 1;
}

const KainUiCompiledNode* kain_ui_compiled_bundle_find_first_kind(
    const KainUiCompiledBundle* bundle,
    KainUiCompiledNodeKind kind
);

void kain_ui_compiled_bundle_init(KainUiCompiledBundle* bundle) {
    if (!bundle) {
        return;
    }
    ZeroMemory(bundle, sizeof(*bundle));
}

int kain_ui_compiled_bundle_load_from_json(const char* json, KainUiCompiledBundle* bundle) {
    const char* json_end;
    const char* output_start;
    const char* output_end;
    const char* tree_start;
    const char* tree_end;
    const char* tree_nodes_start;
    const char* tree_nodes_end;
    const char* cursor;
    const char* tree_node_starts[KAIN_UI_COMPILED_BUNDLE_MAX_NODES];
    const char* tree_node_ends[KAIN_UI_COMPILED_BUNDLE_MAX_NODES];
    int tree_node_index = 0;
    int parsed_canonical_tree = 0;
    int index;

    if (!json || !bundle) {
        return 0;
    }

    kain_ui_compiled_bundle_init(bundle);
    json_end = json + strlen(json);

    kain_ui_extract_string_field(json, json_end, "\"window_title\"", bundle->window_title, sizeof(bundle->window_title));

    ZeroMemory(tree_node_starts, sizeof(tree_node_starts));
    ZeroMemory(tree_node_ends, sizeof(tree_node_ends));

    if (kain_ui_extract_object_field_bounds(json, json_end, "\"output\"", &output_start, &output_end)
        && kain_ui_extract_object_field_bounds(output_start, output_end, "\"tree\"", &tree_start, &tree_end)) {
        if (kain_ui_extract_u64_field(tree_start, tree_end, "\"root\"", &bundle->root_id)) {
            bundle->has_root_id = 1;
        }
        if (kain_ui_extract_object_field_bounds(tree_start, tree_end, "\"nodes\"", &tree_nodes_start, &tree_nodes_end)) {
            cursor = tree_nodes_start + 1;
            while (cursor < tree_nodes_end && bundle->node_count < KAIN_UI_COMPILED_BUNDLE_MAX_NODES) {
                const char* key_start = strchr(cursor, '"');
                const char* key_end;
                const char* value_start;
                const char* value_end;

                if (!key_start || key_start >= tree_nodes_end) {
                    break;
                }
                key_end = strchr(key_start + 1, '"');
                if (!key_end || key_end >= tree_nodes_end) {
                    break;
                }
                value_start = strchr(key_end, ':');
                if (!value_start || value_start >= tree_nodes_end) {
                    break;
                }
                value_start = kain_ui_skip_ws(value_start + 1, tree_nodes_end);
                if (!value_start || value_start >= tree_nodes_end || *value_start != '{') {
                    cursor = key_end + 1;
                    continue;
                }
                value_end = kain_ui_find_matching(value_start, tree_nodes_end, '{', '}');
                if (!value_end) {
                    return 0;
                }

                if (kain_ui_parse_tree_node(value_start, value_end, &bundle->nodes[bundle->node_count])) {
                    tree_node_starts[bundle->node_count] = value_start;
                    tree_node_ends[bundle->node_count] = value_end;
                    bundle->node_count += 1;
                    parsed_canonical_tree = 1;
                }
                cursor = value_end + 1;
            }
        }
    }

    if (parsed_canonical_tree) {
        for (tree_node_index = 0; tree_node_index < bundle->node_count; ++tree_node_index) {
            const char* children_start = NULL;
            const char* children_end = NULL;

            if (!kain_ui_extract_array_field_bounds(
                    tree_node_starts[tree_node_index],
                    tree_node_ends[tree_node_index],
                    "\"children\"",
                    &children_start,
                    &children_end
                )) {
                continue;
            }

            cursor = children_start + 1;
            while (cursor < children_end) {
                char* end_ptr = NULL;
                unsigned long long child_id = _strtoui64(cursor, &end_ptr, 10);
                if (end_ptr && end_ptr > cursor) {
                    int child_index = kain_ui_find_node_index_by_id(bundle, child_id);
                    if (child_index >= 0) {
                        bundle->nodes[child_index].parent_id = bundle->nodes[tree_node_index].id;
                        bundle->nodes[child_index].has_parent = 1;
                    }
                    cursor = end_ptr;
                    continue;
                }
                cursor += 1;
            }
        }

        for (index = 0; index < bundle->node_count; ++index) {
            bundle->nodes[index].depth = kain_ui_compute_node_depth(bundle, index, 0);
        }
    }

    if (!parsed_canonical_tree) {
        return 0;
    }
    if (!bundle->has_root_id) {
        return 0;
    }
    if (kain_ui_find_node_index_by_id(bundle, bundle->root_id) < 0) {
        return 0;
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
