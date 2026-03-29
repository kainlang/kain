#include "../../native/include/kain_ui_runtime.h"
#include <stdio.h>
#include <string.h>

static int g_failed = 0;

static void kain_ui_runtime_test_fill_bundle(KainUiCompiledBundle* bundle) {
    kain_ui_compiled_bundle_init(bundle);
    bundle->loaded = 1;
    bundle->has_root_id = 1;
    bundle->root_id = 1ull;
    snprintf(bundle->window_title, sizeof(bundle->window_title), "%s", "Kain UI Runtime Smoke");
    snprintf(bundle->primary_panel_title, sizeof(bundle->primary_panel_title), "%s", "UI Surface");
    snprintf(bundle->primary_viewport_title, sizeof(bundle->primary_viewport_title), "%s", "Viewport");
    snprintf(bundle->primary_viewport_scene, sizeof(bundle->primary_viewport_scene), "%s", "magma_terraces");

    bundle->node_count = 3;

    bundle->nodes[0].id = 1ull;
    bundle->nodes[0].depth = 0u;
    bundle->nodes[0].kind = KAIN_UI_COMPILED_NODE_PANEL;
    snprintf(bundle->nodes[0].title, sizeof(bundle->nodes[0].title), "%s", "Root Panel");
    snprintf(bundle->nodes[0].text, sizeof(bundle->nodes[0].text), "%s", "compiled overlay");
    snprintf(bundle->nodes[0].tag, sizeof(bundle->nodes[0].tag), "%s", "panel");
    snprintf(bundle->nodes[0].scene, sizeof(bundle->nodes[0].scene), "%s", "magma_terraces");
    snprintf(bundle->nodes[0].layout_kind, sizeof(bundle->nodes[0].layout_kind), "%s", "stack");
    bundle->nodes[0].child_count = 2;

    bundle->nodes[1].id = 2ull;
    bundle->nodes[1].parent_id = 1ull;
    bundle->nodes[1].has_parent = 1;
    bundle->nodes[1].depth = 1u;
    bundle->nodes[1].kind = KAIN_UI_COMPILED_NODE_ELEMENT;
    snprintf(bundle->nodes[1].title, sizeof(bundle->nodes[1].title), "%s", "Name Field");
    snprintf(bundle->nodes[1].text, sizeof(bundle->nodes[1].text), "%s", "Ada");
    snprintf(bundle->nodes[1].tag, sizeof(bundle->nodes[1].tag), "%s", "input");
    snprintf(bundle->nodes[1].scene, sizeof(bundle->nodes[1].scene), "%s", "magma_terraces");
    snprintf(bundle->nodes[1].layout_kind, sizeof(bundle->nodes[1].layout_kind), "%s", "text-entry");
    bundle->nodes[1].child_count = 0;

    bundle->nodes[2].id = 3ull;
    bundle->nodes[2].parent_id = 1ull;
    bundle->nodes[2].has_parent = 1;
    bundle->nodes[2].depth = 1u;
    bundle->nodes[2].kind = KAIN_UI_COMPILED_NODE_VIEWPORT3D;
    snprintf(bundle->nodes[2].title, sizeof(bundle->nodes[2].title), "%s", "Viewport");
    snprintf(bundle->nodes[2].text, sizeof(bundle->nodes[2].text), "%s", "");
    snprintf(bundle->nodes[2].tag, sizeof(bundle->nodes[2].tag), "%s", "viewport");
    snprintf(bundle->nodes[2].scene, sizeof(bundle->nodes[2].scene), "%s", "magma_terraces");
    snprintf(bundle->nodes[2].layout_kind, sizeof(bundle->nodes[2].layout_kind), "%s", "surface");
    bundle->nodes[2].child_count = 0;
}

static void kain_ui_runtime_test_fill_state(KainUiRuntimeState* state, const KainUiCompiledBundle* bundle) {
    kain_ui_runtime_state_init(state);
    state->loaded = 1;
    state->bundle = *bundle;
    state->component_count = 3;
    state->capability_flags = KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS |
        KAIN_UI_RUNTIME_CAP_STATE_PERSISTENCE |
        KAIN_UI_RUNTIME_CAP_OVERLAY_COMPAT |
        KAIN_UI_RUNTIME_CAP_FOCUS_ROUTING |
        KAIN_UI_RUNTIME_CAP_EVENT_ROUTING |
        KAIN_UI_RUNTIME_CAP_EDITABLE_CONTROLS;
    state->validation.valid = 1;
    state->validation.loaded = 1;
    state->validation.overlay_compatible = 1;
    state->validation.root_present = 1;
    state->validation.focusable_count = 3;
    state->validation.editable_count = 1;
    state->validation.component_count = 3;
    state->validation.capability_flags = state->capability_flags;
    snprintf(state->validation.summary, sizeof(state->validation.summary), "%s", "manual ui bundle smoke");

    state->components[0].id = 1ull;
    state->components[0].kind = KAIN_UI_COMPILED_NODE_PANEL;
    state->components[0].focusable = 1;
    state->components[0].capability_flags = KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS;
    snprintf(state->components[0].role, sizeof(state->components[0].role), "%s", "panel");
    snprintf(state->components[0].title, sizeof(state->components[0].title), "%s", "Root Panel");
    snprintf(state->components[0].text, sizeof(state->components[0].text), "%s", "compiled overlay");
    snprintf(state->components[0].value, sizeof(state->components[0].value), "%s", "compiled overlay");
    state->components[0].value_length = strlen(state->components[0].value);

    state->components[1].id = 2ull;
    state->components[1].kind = KAIN_UI_COMPILED_NODE_ELEMENT;
    state->components[1].focusable = 1;
    state->components[1].editable = 1;
    state->components[1].capability_flags = KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS |
        KAIN_UI_RUNTIME_CAP_FOCUS_ROUTING |
        KAIN_UI_RUNTIME_CAP_EDITABLE_CONTROLS;
    snprintf(state->components[1].role, sizeof(state->components[1].role), "%s", "element");
    snprintf(state->components[1].title, sizeof(state->components[1].title), "%s", "Name Field");
    snprintf(state->components[1].text, sizeof(state->components[1].text), "%s", "Ada");
    snprintf(state->components[1].value, sizeof(state->components[1].value), "%s", "Ada");
    state->components[1].value_length = strlen(state->components[1].value);
    state->components[1].cursor = state->components[1].value_length;

    state->components[2].id = 3ull;
    state->components[2].kind = KAIN_UI_COMPILED_NODE_VIEWPORT3D;
    state->components[2].focusable = 1;
    state->components[2].capability_flags = KAIN_UI_RUNTIME_CAP_COMPONENT_RECORDS |
        KAIN_UI_RUNTIME_CAP_FOCUS_ROUTING |
        KAIN_UI_RUNTIME_CAP_OVERLAY_COMPAT;
    snprintf(state->components[2].role, sizeof(state->components[2].role), "%s", "viewport-3d");
    snprintf(state->components[2].title, sizeof(state->components[2].title), "%s", "Viewport");
}

static int test_fail(const char* message) {
    fprintf(stderr, "[FAIL] %s\n", message);
    fflush(stderr);
    g_failed = 1;
    return 0;
}

static int test_true(int condition, const char* message) {
    if (!condition) {
        return test_fail(message);
    }
    return 1;
}

static int test_focus_bundle_uses_canonical_root(const KainUiCompiledBundle* bundle) {
    if (!test_true(bundle->has_root_id, "focus smoke should expose canonical output.tree root")) {
        return 0;
    }
    if (!test_true(bundle->root_id == 1ull, "focus smoke root should match canonical output.tree root")) {
        return 0;
    }
    if (!test_true(bundle->nodes[1].parent_id == bundle->root_id, "editable field should remain parented under canonical root")) {
        return 0;
    }
    if (!test_true(bundle->nodes[2].parent_id == bundle->root_id, "viewport should remain parented under canonical root")) {
        return 0;
    }
    return 1;
}

static int route_event(KainUiRuntimeState* state, KainUiRuntimeEvent* event, KainUiRuntimeEventResult* result) {
    if (!kain_ui_runtime_route_event(state, event, result)) {
        return test_fail("event should have been handled");
    }
    if (!test_true(result->handled, "result should mark event as handled")) {
        return 0;
    }
    return 1;
}

int main(void) {
    KainUiCompiledBundle* bundle;
    KainUiRuntimeState* state;
    KainUiRuntimeEventResult result;
    KainUiRuntimeEvent event;
    const KainUiRuntimeComponentState* focused;

    bundle = (KainUiCompiledBundle*)calloc(1, sizeof(*bundle));
    state = (KainUiRuntimeState*)calloc(1, sizeof(*state));
    if (!bundle || !state) {
        fprintf(stderr, "[FAIL] allocation failed\n");
        fflush(stderr);
        free(bundle);
        free(state);
        return 1;
    }

    kain_ui_runtime_test_fill_bundle(bundle);
    kain_ui_runtime_test_fill_state(state, bundle);
    if (!test_focus_bundle_uses_canonical_root(bundle)) {
        goto cleanup;
    }

    fprintf(stderr, "[focus] request focus\n");
    fflush(stderr);
    {
        int focus_ok = kain_ui_runtime_request_focus(state, 2ull);
        fprintf(stderr, "[focus] focus result=%d focused=%llu active=%llu\n", focus_ok, state->focused_component_id, state->active_edit_component_id);
        fflush(stderr);
        if (!test_true(focus_ok, "explicit focus request should succeed")) {
            goto cleanup;
        }
    }
    fprintf(stderr, "[focus] after focus request\n");
    fflush(stderr);
    if (!test_true(state->focused_component_id == 2ull, "focus should land on editable field")) {
        goto cleanup;
    }
    if (!test_true(state->active_edit_component_id == 2ull, "editable field should become active editor")) {
        goto cleanup;
    }

    fprintf(stderr, "[focus] text input\n");
    fflush(stderr);
    ZeroMemory(&event, sizeof(event));
    event.kind = KAIN_UI_RUNTIME_EVENT_TEXT_INPUT;
    event.target_component_id = 2ull;
    snprintf(event.text, sizeof(event.text), "%s", "Z");
    if (!route_event(state, &event, &result)) {
        goto cleanup;
    }
    if (!test_true(result.edit_changed, "text input should mutate edit state")) {
        goto cleanup;
    }
    if (!test_true(result.dirty_changed, "text input should dirty the component")) {
        goto cleanup;
    }
    focused = kain_ui_runtime_find_component(state, 2ull);
    if (!test_true(focused != NULL, "focused component should still be discoverable")) {
        goto cleanup;
    }
    if (!test_true(strcmp(focused->value, "AdaZ") == 0, "text input should append to the component value")) {
        goto cleanup;
    }
    if (!test_true(state->dirty_component_count > 0u, "dirty component count should advance after edit")) {
        goto cleanup;
    }

    fprintf(stderr, "[focus] backspace\n");
    fflush(stderr);
    ZeroMemory(&event, sizeof(event));
    event.kind = KAIN_UI_RUNTIME_EVENT_KEY_DOWN;
    event.key_code = KAIN_UI_RUNTIME_KEY_BACKSPACE;
    if (!route_event(state, &event, &result)) {
        goto cleanup;
    }
    if (!test_true(strcmp(focused->value, "Ada") == 0, "backspace should remove the last character")) {
        goto cleanup;
    }

    fprintf(stderr, "[focus] tab\n");
    fflush(stderr);
    ZeroMemory(&event, sizeof(event));
    event.kind = KAIN_UI_RUNTIME_EVENT_KEY_DOWN;
    event.key_code = KAIN_UI_RUNTIME_KEY_TAB;
    if (!route_event(state, &event, &result)) {
        goto cleanup;
    }
    if (!test_true(result.focus_changed, "tab should move focus")) {
        goto cleanup;
    }
    if (!test_true(state->focused_component_id == 3ull, "focus should advance to the viewport")) {
        goto cleanup;
    }
    if (!test_true(state->active_edit_component_id == 0ull, "non-editable focus target should clear active editor")) {
        goto cleanup;
    }

    fprintf(stderr, "[focus] pointer down\n");
    fflush(stderr);
    ZeroMemory(&event, sizeof(event));
    event.kind = KAIN_UI_RUNTIME_EVENT_POINTER_DOWN;
    event.target_component_id = 2ull;
    if (!route_event(state, &event, &result)) {
        goto cleanup;
    }
    if (!test_true(state->focused_component_id == 2ull, "pointer routing should restore focus to the editable field")) {
        goto cleanup;
    }
    if (!test_true(state->active_edit_component_id == 2ull, "pointer routing should re-activate editable focus")) {
        goto cleanup;
    }

    fprintf(stderr, "[focus] blur\n");
    fflush(stderr);
    ZeroMemory(&event, sizeof(event));
    event.kind = KAIN_UI_RUNTIME_EVENT_BLUR;
    if (!route_event(state, &event, &result)) {
        goto cleanup;
    }
    if (!test_true(state->focused_component_id == 0ull, "blur should clear focus")) {
        goto cleanup;
    }
    if (!test_true(state->active_edit_component_id == 0ull, "blur should clear active editor")) {
        goto cleanup;
    }

    fprintf(stderr, "[focus] final pass\n");
    fflush(stderr);
    printf("[PASS] ui runtime focus and routing smoke\n");

cleanup:
    free(bundle);
    free(state);
    return g_failed ? 1 : 0;
}
