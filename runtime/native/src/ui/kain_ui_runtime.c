#include "../../include/kain_ui_runtime.h"
#include "../../include/kain_runtime_version.h"

typedef struct {
    KainUiCompiledNodeKind kind;
    const char* role;
    unsigned int capability_flags;
    int focusable;
    int editable;
} KainUiRuntimeKindProfile;

static const KainUiRuntimeKindProfile g_kain_ui_kind_profiles[] = {
    { KAIN_UI_COMPILED_NODE_PANEL, "panel", KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS | KAIN_UI_RUNTIME_CAP_OVERLAY_COMPAT, 1, 0 },
    { KAIN_UI_COMPILED_NODE_INSPECTOR, "inspector", KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS | KAIN_UI_RUNTIME_CAP_OVERLAY_COMPAT, 1, 0 },
    { KAIN_UI_COMPILED_NODE_GRAPH, "graph", KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS, 1, 0 },
    { KAIN_UI_COMPILED_NODE_TIMELINE, "timeline", KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS, 1, 0 },
    { KAIN_UI_COMPILED_NODE_TABLE, "table", KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS, 1, 0 },
    { KAIN_UI_COMPILED_NODE_TREE, "tree", KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS, 1, 0 },
    { KAIN_UI_COMPILED_NODE_VIEWPORT2D, "viewport-2d", KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS | KAIN_UI_RUNTIME_CAP_FOCUS_ROUTING, 1, 0 },
    { KAIN_UI_COMPILED_NODE_VIEWPORT3D, "viewport-3d", KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS | KAIN_UI_RUNTIME_CAP_FOCUS_ROUTING | KAIN_UI_RUNTIME_CAP_OVERLAY_COMPAT, 1, 0 },
    { KAIN_UI_COMPILED_NODE_OVERLAY, "overlay", KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS | KAIN_UI_RUNTIME_CAP_OVERLAY_COMPAT, 0, 0 },
    { KAIN_UI_COMPILED_NODE_SLOT, "slot", KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS, 1, 0 },
    { KAIN_UI_COMPILED_NODE_ELEMENT, "element", KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS, 1, 0 },
    { KAIN_UI_COMPILED_NODE_COMPONENT_REF, "component-ref", KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS, 1, 0 },
    { KAIN_UI_COMPILED_NODE_TEXT, "text", KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS, 0, 0 },
    { KAIN_UI_COMPILED_NODE_UNKNOWN, "unknown", 0u, 0, 0 },
};

static const char* const g_kain_ui_focusable_tags[] = {
    "button",
    "control",
    "field",
    "graph",
    "input",
    "inspector",
    "panel",
    "search",
    "slider",
    "table",
    "textbox",
    "text-input",
    "timeline",
    "tree",
    "viewport",
    "viewport2d",
    "viewport3d",
    "editable",
};

static const char* const g_kain_ui_editable_tags[] = {
    "editable",
    "field",
    "input",
    "search",
    "text-input",
    "textbox",
    "textarea",
    "value",
};

static const char* const g_kain_ui_editable_layouts[] = {
    "editor",
    "field",
    "input",
    "text",
    "text-entry",
    "textbox",
    "textarea",
};

static const KainUiRuntimeKindProfile* kain_ui_runtime_lookup_kind_profile(KainUiCompiledNodeKind kind) {
    size_t index;

    for (index = 0; index < sizeof(g_kain_ui_kind_profiles) / sizeof(g_kain_ui_kind_profiles[0]); ++index) {
        if (g_kain_ui_kind_profiles[index].kind == kind) {
            return &g_kain_ui_kind_profiles[index];
        }
    }

    return &g_kain_ui_kind_profiles[sizeof(g_kain_ui_kind_profiles) / sizeof(g_kain_ui_kind_profiles[0]) - 1];
}

static int kain_ui_runtime_string_is_one_of(const char* value, const char* const* candidates, size_t candidate_count) {
    size_t index;

    if (!value || !value[0] || !candidates) {
        return 0;
    }

    for (index = 0; index < candidate_count; ++index) {
        if (candidates[index] && _stricmp(value, candidates[index]) == 0) {
            return 1;
        }
    }

    return 0;
}

static const char* kain_ui_runtime_node_role(const KainUiCompiledNode* node) {
    const KainUiRuntimeKindProfile* profile;

    if (!node) {
        return "unknown";
    }

    profile = kain_ui_runtime_lookup_kind_profile(node->kind);
    if (node->tag[0] && _stricmp(node->tag, "overlay") == 0) {
        return "overlay";
    }
    if (node->layout_kind[0] && _stricmp(node->layout_kind, "editor") == 0) {
        return "editor";
    }
    return profile->role;
}

static int kain_ui_runtime_node_is_focusable(const KainUiCompiledNode* node) {
    const KainUiRuntimeKindProfile* profile;

    if (!node) {
        return 0;
    }

    profile = kain_ui_runtime_lookup_kind_profile(node->kind);
    if (profile->focusable) {
        return 1;
    }

    if (kain_ui_runtime_string_is_one_of(node->tag, g_kain_ui_focusable_tags, sizeof(g_kain_ui_focusable_tags) / sizeof(g_kain_ui_focusable_tags[0]))) {
        return 1;
    }

    if (kain_ui_runtime_string_is_one_of(node->layout_kind, g_kain_ui_focusable_tags, sizeof(g_kain_ui_focusable_tags) / sizeof(g_kain_ui_focusable_tags[0]))) {
        return 1;
    }

    return 0;
}

static int kain_ui_runtime_node_is_editable(const KainUiCompiledNode* node) {
    const KainUiRuntimeKindProfile* profile;

    if (!node) {
        return 0;
    }

    profile = kain_ui_runtime_lookup_kind_profile(node->kind);
    if (profile->editable) {
        return 1;
    }

    if (kain_ui_runtime_string_is_one_of(node->tag, g_kain_ui_editable_tags, sizeof(g_kain_ui_editable_tags) / sizeof(g_kain_ui_editable_tags[0]))) {
        return 1;
    }

    if (kain_ui_runtime_string_is_one_of(node->layout_kind, g_kain_ui_editable_layouts, sizeof(g_kain_ui_editable_layouts) / sizeof(g_kain_ui_editable_layouts[0]))) {
        return 1;
    }

    return node->kind == KAIN_UI_COMPILED_NODE_ELEMENT && node->tag[0] && _stricmp(node->tag, "input") == 0;
}

static void kain_ui_runtime_copy_string(char* out, size_t out_cap, const char* value) {
    if (!out || out_cap == 0) {
        return;
    }

    if (!value) {
        out[0] = '\0';
        return;
    }

    snprintf(out, out_cap, "%s", value);
}

static void kain_ui_runtime_validation_init_inner(KainUiRuntimeValidationReport* report) {
    if (!report) {
        return;
    }

    ZeroMemory(report, sizeof(*report));
}

static void kain_ui_runtime_report_add(
    KainUiRuntimeValidationReport* report,
    KainDiagSeverity severity,
    int code,
    const char* message,
    const char* detail
) {
    KainDiagnostic* diag;

    if (!report || report->issue_count >= KAIN_UI_RUNTIME_MAX_ISSUES) {
        return;
    }

    diag = &report->issues[report->issue_count];
    ZeroMemory(diag, sizeof(*diag));
    diag->subsystem = KAIN_DIAG_SUBSYSTEM_UI;
    diag->severity = severity;
    diag->code = code;
    diag->runtime_abi_version = KAIN_RUNTIME_ABI_VERSION_CURRENT;
    if (message) {
        snprintf(diag->message, sizeof(diag->message), "%s", message);
    }
    if (detail) {
        snprintf(diag->detail, sizeof(diag->detail), "%s", detail);
    }
    report->issue_count += 1;
    if (severity >= KAIN_DIAG_SEVERITY_ERROR) {
        report->error_count += 1;
    } else if (severity == KAIN_DIAG_SEVERITY_WARNING) {
        report->warning_count += 1;
    }
}

static int kain_ui_runtime_find_node_index(const KainUiCompiledBundle* bundle, unsigned long long id) {
    int index;

    if (!bundle || !bundle->loaded) {
        return -1;
    }

    for (index = 0; index < bundle->node_count; ++index) {
        if (bundle->nodes[index].id == id) {
            return index;
        }
    }

    return -1;
}

static int kain_ui_runtime_build_component_state(
    const KainUiCompiledBundle* bundle,
    const KainUiCompiledNode* node,
    int node_index,
    KainUiRuntimeComponentState* out_state
) {
    const KainUiRuntimeKindProfile* profile;

    if (!bundle || !node || !out_state) {
        return 0;
    }

    ZeroMemory(out_state, sizeof(*out_state));
    profile = kain_ui_runtime_lookup_kind_profile(node->kind);
    out_state->id = node->id;
    out_state->parent_id = node->parent_id;
    out_state->has_parent = node->has_parent;
    out_state->depth = node->depth;
    out_state->node_index = node_index;
    out_state->kind = node->kind;
    out_state->capability_flags = profile->capability_flags;
    out_state->focusable = kain_ui_runtime_node_is_focusable(node);
    out_state->editable = kain_ui_runtime_node_is_editable(node);
    if (out_state->focusable) {
        out_state->capability_flags |= KAIN_UI_RUNTIME_CAP_FOCUS_ROUTING;
    }
    if (out_state->editable) {
        out_state->capability_flags |= KAIN_UI_RUNTIME_CAP_EDITABLE_CONTROLS | KAIN_UI_RUNTIME_CAP_EVENT_ROUTING;
    }
    out_state->revision = 1u;
    out_state->dirty_reason_mask = 0u;
    out_state->last_event_kind = KAIN_UI_RUNTIME_EVENT_NONE;
    kain_ui_runtime_copy_string(out_state->role, sizeof(out_state->role), kain_ui_runtime_node_role(node));
    kain_ui_runtime_copy_string(out_state->title, sizeof(out_state->title), node->title);
    kain_ui_runtime_copy_string(out_state->text, sizeof(out_state->text), node->text);
    kain_ui_runtime_copy_string(out_state->tag, sizeof(out_state->tag), node->tag);
    kain_ui_runtime_copy_string(out_state->scene, sizeof(out_state->scene), node->scene);
    kain_ui_runtime_copy_string(out_state->layout_kind, sizeof(out_state->layout_kind), node->layout_kind);
    kain_ui_runtime_copy_string(out_state->value, sizeof(out_state->value), node->text);
    out_state->value_length = strlen(out_state->value);
    out_state->cursor = out_state->value_length;
    return 1;
}

static void kain_ui_runtime_refresh_state_capabilities(KainUiRuntimeState* state) {
    int index;
    unsigned int capability_flags = KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS | KAIN_UI_RUNTIME_CAP_STATE_PERSISTENCE;
    int focusable_count = 0;
    int editable_count = 0;

    if (!state) {
        return;
    }

    for (index = 0; index < state->component_count; ++index) {
        capability_flags |= state->components[index].capability_flags;
        if (state->components[index].focusable) {
            focusable_count += 1;
        }
        if (state->components[index].editable) {
            editable_count += 1;
        }
    }

    if (focusable_count > 0) {
        capability_flags |= KAIN_UI_RUNTIME_CAP_FOCUS_ROUTING | KAIN_UI_RUNTIME_CAP_EVENT_ROUTING;
    }
    if (editable_count > 0) {
        capability_flags |= KAIN_UI_RUNTIME_CAP_EDITABLE_CONTROLS;
    }

    state->capability_flags = capability_flags;
    state->validation.focusable_count = focusable_count;
    state->validation.editable_count = editable_count;
}

static int kain_ui_runtime_validate_bundle_inner(
    const KainUiCompiledBundle* bundle,
    KainUiRuntimeValidationReport* report
) {
    int index;
    int root_index = -1;
    int focusable_count = 0;
    int editable_count = 0;
    int has_overlay_kind = 0;

    kain_ui_runtime_validation_init_inner(report);
    if (!bundle) {
        kain_ui_runtime_report_add(
            report,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_UI_BUNDLE_INVALID_SCHEMA,
            "ui bundle missing",
            "bundle pointer was NULL"
        );
        return 0;
    }

    report->loaded = bundle->loaded;
    if (!bundle->loaded) {
        kain_ui_runtime_report_add(
            report,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_UI_BUNDLE_PARSE_FAILED,
            "ui bundle not loaded",
            "compiled bundle did not report loaded=1"
        );
    }
    if (!bundle->has_root_id) {
        kain_ui_runtime_report_add(
            report,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_UI_BUNDLE_INVALID_SCHEMA,
            "ui bundle missing root",
            "canonical output.tree.root was not set"
        );
    }
    if (!bundle->window_title[0]) {
        kain_ui_runtime_report_add(
            report,
            KAIN_DIAG_SEVERITY_WARNING,
            KAIN_DIAG_CODE_UI_BUNDLE_INVALID_SCHEMA,
            "ui bundle window title missing",
            "window_title is empty"
        );
    }

    for (index = 0; index < bundle->node_count; ++index) {
        const KainUiCompiledNode* node = &bundle->nodes[index];
        int other_index;

        if (bundle->has_root_id && node->id == bundle->root_id) {
            root_index = index;
            report->root_present = 1;
        }
        if (node->kind == KAIN_UI_COMPILED_NODE_PANEL ||
            node->kind == KAIN_UI_COMPILED_NODE_INSPECTOR ||
            node->kind == KAIN_UI_COMPILED_NODE_VIEWPORT2D ||
            node->kind == KAIN_UI_COMPILED_NODE_VIEWPORT3D ||
            node->kind == KAIN_UI_COMPILED_NODE_OVERLAY) {
            has_overlay_kind = 1;
        }
        if (kain_ui_runtime_node_is_focusable(node)) {
            focusable_count += 1;
        }
        if (kain_ui_runtime_node_is_editable(node)) {
            editable_count += 1;
        }

        for (other_index = index + 1; other_index < bundle->node_count; ++other_index) {
            if (bundle->nodes[other_index].id == node->id) {
                char detail[128];
                snprintf(detail, sizeof(detail), "duplicate node id %llu", node->id);
                kain_ui_runtime_report_add(
                    report,
                    KAIN_DIAG_SEVERITY_ERROR,
                    KAIN_DIAG_CODE_UI_BUNDLE_INVALID_SCHEMA,
                    "duplicate ui node id",
                    detail
                );
                break;
            }
        }
    }

    if (bundle->has_root_id && root_index < 0) {
        kain_ui_runtime_report_add(
            report,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_UI_BUNDLE_INVALID_SCHEMA,
            "root node missing",
            "root_id did not match any node"
        );
    }

    for (index = 0; index < bundle->node_count; ++index) {
        const KainUiCompiledNode* node = &bundle->nodes[index];
        if (node->has_parent) {
            int parent_index = kain_ui_runtime_find_node_index(bundle, node->parent_id);
            if (parent_index < 0) {
                char detail[128];
                snprintf(detail, sizeof(detail), "node %llu references missing parent %llu", node->id, node->parent_id);
                kain_ui_runtime_report_add(
                    report,
                    KAIN_DIAG_SEVERITY_ERROR,
                    KAIN_DIAG_CODE_UI_BUNDLE_INVALID_SCHEMA,
                    "orphan ui node parent",
                    detail
                );
            }
        }
    }

    if (focusable_count == 0) {
        kain_ui_runtime_report_add(
            report,
            KAIN_DIAG_SEVERITY_WARNING,
            KAIN_DIAG_CODE_UI_COMPONENT_INIT_FAILED,
            "no focusable ui components",
            "focus routing groundwork has nothing to target yet"
        );
    }
    if (editable_count == 0) {
        kain_ui_runtime_report_add(
            report,
            KAIN_DIAG_SEVERITY_INFO,
            KAIN_DIAG_CODE_UI_COMPONENT_INIT_FAILED,
            "no editable ui components",
            "editable-control plumbing is present but no editable nodes exist"
        );
    }

    report->component_count = bundle->node_count;
    report->focusable_count = focusable_count;
    report->editable_count = editable_count;
    report->overlay_compatible = bundle->loaded && has_overlay_kind;
    report->capability_flags = KAIN_UI_RUNTIME_CAP_BUNDLE_VALIDATED | KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS | KAIN_UI_RUNTIME_CAP_STATE_PERSISTENCE;
    if (report->overlay_compatible) {
        report->capability_flags |= KAIN_UI_RUNTIME_CAP_OVERLAY_COMPAT;
    }
    if (focusable_count > 0) {
        report->capability_flags |= KAIN_UI_RUNTIME_CAP_FOCUS_ROUTING | KAIN_UI_RUNTIME_CAP_EVENT_ROUTING;
    }
    if (editable_count > 0) {
        report->capability_flags |= KAIN_UI_RUNTIME_CAP_EDITABLE_CONTROLS;
    }
    report->valid = (report->error_count == 0);
    snprintf(
        report->summary,
        sizeof(report->summary),
        "loaded=%d nodes=%d focusable=%d editable=%d overlay=%d",
        report->loaded,
        report->component_count,
        report->focusable_count,
        report->editable_count,
        report->overlay_compatible
    );
    return report->valid;
}

static int kain_ui_runtime_rebuild_components(KainUiRuntimeState* state) {
    int index;

    if (!state || !state->bundle.loaded) {
        return 0;
    }

    state->component_count = 0;
    for (index = 0; index < state->bundle.node_count && state->component_count < KAIN_UI_RUNTIME_MAX_COMPONENTS; ++index) {
        if (kain_ui_runtime_build_component_state(
            &state->bundle,
            &state->bundle.nodes[index],
            index,
            &state->components[state->component_count]
        )) {
            state->component_count += 1;
        }
    }

    kain_ui_runtime_refresh_state_capabilities(state);
    return 1;
}

static int kain_ui_runtime_set_focus_by_index(KainUiRuntimeState* state, int index) {
    unsigned long long focus_id;

    if (!state || index < 0 || index >= state->component_count) {
        return 0;
    }

    if (!state->components[index].focusable && !state->components[index].editable) {
        return 0;
    }

    focus_id = state->components[index].id;
    if (state->focused_component_id != focus_id) {
        state->focused_component_id = focus_id;
        state->sequence += 1u;
    }
    if (state->components[index].editable) {
        state->active_edit_component_id = focus_id;
    } else {
        state->active_edit_component_id = 0ull;
    }
    return 1;
}

static int kain_ui_runtime_find_focused_index(const KainUiRuntimeState* state) {
    int index;

    if (!state || !state->focused_component_id) {
        return -1;
    }

    for (index = 0; index < state->component_count; ++index) {
        if (state->components[index].id == state->focused_component_id) {
            return index;
        }
    }

    return -1;
}

static int kain_ui_runtime_find_next_focusable_index(const KainUiRuntimeState* state, int start_index, int step) {
    int passes;
    int index;

    if (!state || state->component_count <= 0) {
        return -1;
    }

    index = start_index;
    for (passes = 0; passes < state->component_count; ++passes) {
        index = (index + step + state->component_count) % state->component_count;
        if (state->components[index].focusable || state->components[index].editable) {
            return index;
        }
    }

    return -1;
}

static int kain_ui_runtime_append_text(KainUiRuntimeComponentState* component, const char* text) {
    size_t current_len;
    size_t append_len;
    size_t room;

    if (!component || !text || !text[0]) {
        return 0;
    }

    current_len = strlen(component->value);
    append_len = strlen(text);
    room = sizeof(component->value) - 1u - current_len;
    if (room == 0) {
        return 0;
    }
    if (append_len > room) {
        append_len = room;
    }
    memcpy(component->value + current_len, text, append_len);
    component->value[current_len + append_len] = '\0';
    component->value_length = strlen(component->value);
    component->cursor = component->value_length;
    component->revision += 1u;
    component->dirty = 1;
    return 1;
}

static int kain_ui_runtime_delete_last_char(KainUiRuntimeComponentState* component) {
    size_t len;

    if (!component) {
        return 0;
    }

    len = strlen(component->value);
    if (len == 0) {
        return 0;
    }

    component->value[len - 1] = '\0';
    component->value_length = len - 1u;
    component->cursor = component->value_length;
    component->revision += 1u;
    component->dirty = 1;
    return 1;
}

static KainUiRuntimeComponentState* kain_ui_runtime_active_component(KainUiRuntimeState* state) {
    int index = kain_ui_runtime_find_focused_index(state);
    if (index < 0) {
        return NULL;
    }
    return &state->components[index];
}

static int kain_ui_runtime_route_text_event(KainUiRuntimeState* state, const KainUiRuntimeEvent* event, KainUiRuntimeEventResult* result) {
    KainUiRuntimeComponentState* component;

    if (!state || !event || !result) {
        return 0;
    }

    component = kain_ui_runtime_active_component(state);
    if (!component || !component->editable) {
        return 0;
    }

    if (event->kind == KAIN_UI_RUNTIME_EVENT_TEXT_INPUT) {
        if (kain_ui_runtime_append_text(component, event->text)) {
            kain_ui_runtime_mark_dirty(state, component->id, 1u << 0);
            result->handled = 1;
            result->edit_changed = 1;
            result->dirty_changed = 1;
            return 1;
        }
        return 0;
    }

    if (event->kind == KAIN_UI_RUNTIME_EVENT_KEY_DOWN) {
        if (event->key_code == KAIN_UI_RUNTIME_KEY_BACKSPACE || event->key_code == KAIN_UI_RUNTIME_KEY_DELETE) {
            if (kain_ui_runtime_delete_last_char(component)) {
                kain_ui_runtime_mark_dirty(state, component->id, 1u << 0);
                result->handled = 1;
                result->edit_changed = 1;
                result->dirty_changed = 1;
                return 1;
            }
        }
        if (event->key_code == KAIN_UI_RUNTIME_KEY_ENTER) {
            result->handled = 1;
            return 1;
        }
    }

    return 0;
}

static int kain_ui_runtime_route_focus_event(KainUiRuntimeState* state, const KainUiRuntimeEvent* event, KainUiRuntimeEventResult* result) {
    int focus_index;

    if (!state || !event || !result) {
        return 0;
    }

    if (event->kind == KAIN_UI_RUNTIME_EVENT_BLUR) {
        if (kain_ui_runtime_clear_focus(state)) {
            result->handled = 1;
            result->focus_changed = 1;
            return 1;
        }
        return 0;
    }

    if (event->kind == KAIN_UI_RUNTIME_EVENT_FOCUS_REQUEST || event->kind == KAIN_UI_RUNTIME_EVENT_POINTER_DOWN) {
        if (event->target_component_id != 0ull) {
            if (kain_ui_runtime_request_focus(state, event->target_component_id)) {
                kain_ui_runtime_mark_dirty(state, event->target_component_id, 1u << 1);
                result->handled = 1;
                result->focus_changed = 1;
                return 1;
            }
        }
        return 0;
    }

    focus_index = kain_ui_runtime_find_focused_index(state);
    if (event->kind == KAIN_UI_RUNTIME_EVENT_FOCUS_NEXT) {
        if (focus_index < 0) {
            focus_index = -1;
        }
        focus_index = kain_ui_runtime_find_next_focusable_index(state, focus_index, 1);
        if (focus_index >= 0 && kain_ui_runtime_set_focus_by_index(state, focus_index)) {
            kain_ui_runtime_mark_dirty(state, state->components[focus_index].id, 1u << 1);
            result->handled = 1;
            result->focus_changed = 1;
            return 1;
        }
    } else if (event->kind == KAIN_UI_RUNTIME_EVENT_FOCUS_PREV) {
        if (focus_index < 0) {
            focus_index = 0;
        }
        focus_index = kain_ui_runtime_find_next_focusable_index(state, focus_index, -1);
        if (focus_index >= 0 && kain_ui_runtime_set_focus_by_index(state, focus_index)) {
            kain_ui_runtime_mark_dirty(state, state->components[focus_index].id, 1u << 1);
            result->handled = 1;
            result->focus_changed = 1;
            return 1;
        }
    }

    return 0;
}

static int kain_ui_runtime_route_key_non_text_event(KainUiRuntimeState* state, const KainUiRuntimeEvent* event, KainUiRuntimeEventResult* result) {
    if (!state || !event || !result) {
        return 0;
    }

    if (event->kind == KAIN_UI_RUNTIME_EVENT_KEY_DOWN && event->key_code == KAIN_UI_RUNTIME_KEY_TAB) {
        KainUiRuntimeEvent synthetic;
        ZeroMemory(&synthetic, sizeof(synthetic));
        synthetic.kind = (event->modifiers & 0x1u) ? KAIN_UI_RUNTIME_EVENT_FOCUS_PREV : KAIN_UI_RUNTIME_EVENT_FOCUS_NEXT;
        return kain_ui_runtime_route_focus_event(state, &synthetic, result);
    }

    if (event->kind == KAIN_UI_RUNTIME_EVENT_EDIT_COMMIT) {
        result->handled = 1;
        return 1;
    }
    if (event->kind == KAIN_UI_RUNTIME_EVENT_EDIT_CANCEL) {
        if (kain_ui_runtime_clear_focus(state)) {
            result->handled = 1;
            result->focus_changed = 1;
            return 1;
        }
    }

    return 0;
}

void kain_ui_runtime_validation_init(KainUiRuntimeValidationReport* report) {
    kain_ui_runtime_validation_init_inner(report);
}

void kain_ui_runtime_state_init(KainUiRuntimeState* state) {
    if (!state) {
        return;
    }

    ZeroMemory(state, sizeof(*state));
    kain_ui_compiled_bundle_init(&state->bundle);
    kain_ui_runtime_validation_init(&state->validation);
    state->initialized = 1;
}

int kain_ui_runtime_validate_bundle(const KainUiCompiledBundle* bundle, KainUiRuntimeValidationReport* report) {
    return kain_ui_runtime_validate_bundle_inner(bundle, report);
}

int kain_ui_runtime_state_load_bundle(KainUiRuntimeState* state, const KainUiCompiledBundle* bundle) {
    int valid;

    if (!state || !bundle) {
        return 0;
    }

    kain_ui_runtime_state_init(state);
    state->bundle = *bundle;
    valid = kain_ui_runtime_validate_bundle(&state->bundle, &state->validation);
    if (!state->bundle.loaded) {
        state->loaded = 0;
        return 0;
    }

    state->loaded = 1;
    kain_ui_runtime_rebuild_components(state);
    return valid;
}

int kain_ui_runtime_state_load_from_json(KainUiRuntimeState* state, const char* json) {
    KainUiCompiledBundle bundle;

    if (!state || !json) {
        return 0;
    }

    kain_ui_compiled_bundle_init(&bundle);
    if (!kain_ui_compiled_bundle_load_from_json(json, &bundle)) {
        kain_ui_runtime_state_init(state);
        return 0;
    }

    return kain_ui_runtime_state_load_bundle(state, &bundle);
}

int kain_ui_runtime_state_load_from_path(KainUiRuntimeState* state, const char* path) {
    KainUiCompiledBundle bundle;

    if (!state || !path) {
        return 0;
    }

    kain_ui_compiled_bundle_init(&bundle);
    if (!kain_ui_compiled_bundle_load_from_path(path, &bundle)) {
        kain_ui_runtime_state_init(state);
        return 0;
    }

    return kain_ui_runtime_state_load_bundle(state, &bundle);
}

int kain_ui_runtime_state_load_from_env(KainUiRuntimeState* state, const char* env_name) {
    KainUiCompiledBundle bundle;

    if (!state || !env_name) {
        return 0;
    }

    kain_ui_compiled_bundle_init(&bundle);
    if (!kain_ui_compiled_bundle_load_from_env(env_name, &bundle)) {
        kain_ui_runtime_state_init(state);
        return 0;
    }

    return kain_ui_runtime_state_load_bundle(state, &bundle);
}

const KainUiRuntimeComponentState* kain_ui_runtime_find_component(
    const KainUiRuntimeState* state,
    unsigned long long component_id
) {
    int index;

    if (!state || !state->loaded) {
        return NULL;
    }

    for (index = 0; index < state->component_count; ++index) {
        if (state->components[index].id == component_id) {
            return &state->components[index];
        }
    }

    return NULL;
}

const KainUiRuntimeComponentState* kain_ui_runtime_find_first_kind(
    const KainUiRuntimeState* state,
    KainUiCompiledNodeKind kind
) {
    int index;

    if (!state || !state->loaded) {
        return NULL;
    }

    for (index = 0; index < state->component_count; ++index) {
        if (state->components[index].kind == kind) {
            return &state->components[index];
        }
    }

    return NULL;
}

const KainUiRuntimeComponentState* kain_ui_runtime_find_first_focusable(const KainUiRuntimeState* state) {
    int index;

    if (!state || !state->loaded) {
        return NULL;
    }

    for (index = 0; index < state->component_count; ++index) {
        if (state->components[index].focusable || state->components[index].editable) {
            return &state->components[index];
        }
    }

    return NULL;
}

const KainUiRuntimeComponentState* kain_ui_runtime_find_first_editable(const KainUiRuntimeState* state) {
    int index;

    if (!state || !state->loaded) {
        return NULL;
    }

    for (index = 0; index < state->component_count; ++index) {
        if (state->components[index].editable) {
            return &state->components[index];
        }
    }

    return NULL;
}

int kain_ui_runtime_request_focus(KainUiRuntimeState* state, unsigned long long component_id) {
    int index;

    if (!state || !state->loaded) {
        return 0;
    }

    for (index = 0; index < state->component_count; ++index) {
        if (state->components[index].id == component_id) {
            return kain_ui_runtime_set_focus_by_index(state, index);
        }
    }

    return 0;
}

int kain_ui_runtime_clear_focus(KainUiRuntimeState* state) {
    if (!state || !state->loaded) {
        return 0;
    }

    if (!state->focused_component_id && !state->active_edit_component_id) {
        return 0;
    }

    state->focused_component_id = 0ull;
    state->active_edit_component_id = 0ull;
    state->sequence += 1u;
    return 1;
}

int kain_ui_runtime_mark_dirty(
    KainUiRuntimeState* state,
    unsigned long long component_id,
    unsigned int dirty_reason_mask
) {
    KainUiRuntimeComponentState* component;

    if (!state || !state->loaded) {
        return 0;
    }

    component = (KainUiRuntimeComponentState*)kain_ui_runtime_find_component(state, component_id);
    if (!component) {
        return 0;
    }

    component->dirty = 1;
    component->dirty_reason_mask |= dirty_reason_mask;
    component->revision += 1u;
    state->dirty_component_count += 1u;
    state->sequence += 1u;
    return 1;
}

int kain_ui_runtime_route_event(
    KainUiRuntimeState* state,
    const KainUiRuntimeEvent* event,
    KainUiRuntimeEventResult* result
) {
    int handled;

    if (!state || !state->loaded || !event || !result) {
        return 0;
    }

    ZeroMemory(result, sizeof(*result));
    result->routed_event_kind = (unsigned int)event->kind;
    result->target_component_id = event->target_component_id;

    handled = kain_ui_runtime_route_focus_event(state, event, result);
    if (!handled) {
        handled = kain_ui_runtime_route_text_event(state, event, result);
    }
    if (!handled) {
        handled = kain_ui_runtime_route_key_non_text_event(state, event, result);
    }

    if (handled) {
        KainUiRuntimeComponentState* active = kain_ui_runtime_active_component(state);
        result->handled = 1;
        if (active) {
            result->focused_component_id = state->focused_component_id;
            result->editable_component_id = state->active_edit_component_id;
            if (active->dirty) {
                result->dirty_changed = 1;
            }
        }
        state->sequence += 1u;
        return 1;
    }

    if (result->note[0] == '\0') {
        snprintf(result->note, sizeof(result->note), "event %u not handled", (unsigned int)event->kind);
    }
    return 0;
}

int kain_ui_runtime_has_capability(const KainUiRuntimeState* state, unsigned int capability_mask) {
    if (!state || !state->loaded) {
        return 0;
    }

    return (state->capability_flags & capability_mask) == capability_mask;
}

unsigned int kain_ui_runtime_state_capabilities(const KainUiRuntimeState* state) {
    if (!state || !state->loaded) {
        return 0u;
    }

    return state->capability_flags;
}
