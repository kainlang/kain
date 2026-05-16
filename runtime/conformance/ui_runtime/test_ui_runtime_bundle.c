#include "../../native/include/ui_runtime.h"
#include <stdio.h>
#include <string.h>

static int g_failed = 0;

static void kain_ui_runtime_test_fill_bundle(KainUiCompiledBundle* bundle) {
    kain_ui_compiled_bundle_init(bundle);
    bundle->loaded = 1;
    bundle->has_root_id = 1;
    bundle->root_id = 1ull;
    snprintf(bundle->window_title, sizeof(bundle->window_title), "%s", "Kain UI Runtime Smoke");

    bundle->node_count = 3;

    bundle->nodes[0].id = 1ull;
    bundle->nodes[0].depth = 0u;
    bundle->nodes[0].kind = KAIN_UI_COMPILED_NODE_PANEL;
    snprintf(bundle->nodes[0].title, sizeof(bundle->nodes[0].title), "%s", "Root Panel");
    snprintf(bundle->nodes[0].text, sizeof(bundle->nodes[0].text), "%s", "compiled overlay");
    snprintf(bundle->nodes[0].tag, sizeof(bundle->nodes[0].tag), "%s", "panel");
    snprintf(bundle->nodes[0].scene, sizeof(bundle->nodes[0].scene), "%s", "geometry_fixture");
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
    snprintf(bundle->nodes[1].scene, sizeof(bundle->nodes[1].scene), "%s", "geometry_fixture");
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
    snprintf(bundle->nodes[2].scene, sizeof(bundle->nodes[2].scene), "%s", "geometry_fixture");
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

static int test_bundle_root_and_nodes_are_canonical(const KainUiCompiledBundle* bundle) {
    if (!test_true(bundle->has_root_id, "bundle should expose canonical output.tree root")) {
        return 0;
    }
    if (!test_true(bundle->root_id == 1ull, "bundle root id should match canonical output.tree root")) {
        return 0;
    }
    if (!test_true(bundle->node_count == 3, "bundle should expose three canonical output.tree nodes")) {
        return 0;
    }
    if (!test_true(bundle->nodes[0].id == bundle->root_id, "root component should stay first canonical node")) {
        return 0;
    }
    if (!test_true(bundle->nodes[0].kind == KAIN_UI_COMPILED_NODE_PANEL, "canonical root node should be a panel")) {
        return 0;
    }
    if (!test_true(bundle->nodes[1].parent_id == bundle->root_id, "second node should preserve canonical parent")) {
        return 0;
    }
    if (!test_true(bundle->nodes[2].parent_id == bundle->root_id, "third node should preserve canonical parent")) {
        return 0;
    }
    return 1;
}

int main(void) {
    KainUiCompiledBundle* bundle;
    KainUiRuntimeValidationReport* report;
    KainUiRuntimeState* state;
    const KainUiRuntimeComponentState* editable;
    const KainUiRuntimeComponentState* viewport;

    bundle = (KainUiCompiledBundle*)calloc(1, sizeof(*bundle));
    report = (KainUiRuntimeValidationReport*)calloc(1, sizeof(*report));
    state = (KainUiRuntimeState*)calloc(1, sizeof(*state));
    if (!bundle || !report || !state) {
        fprintf(stderr, "[FAIL] allocation failed\n");
        fflush(stderr);
        free(bundle);
        free(report);
        free(state);
        return 1;
    }

    kain_ui_runtime_test_fill_bundle(bundle);
    if (!test_true(bundle->loaded, "bundle should be marked loaded")) {
        goto cleanup;
    }
    if (!test_bundle_root_and_nodes_are_canonical(bundle)) {
        goto cleanup;
    }

    kain_ui_runtime_validation_init(report);
    if (!test_true(kain_ui_runtime_validate_bundle(bundle, report), "bundle should validate")) {
        goto cleanup;
    }
    if (!test_true(report->valid, "validation report should be valid")) {
        goto cleanup;
    }
    if (!test_true(report->overlay_compatible, "bundle should be overlay compatible")) {
        goto cleanup;
    }
    if (!test_true(report->component_count == 3, "validation should count all canonical output.tree components")) {
        goto cleanup;
    }
    if (!test_true(report->focusable_count >= 2, "validation should detect focusable components")) {
        goto cleanup;
    }
    if (!test_true(report->editable_count == 1, "validation should detect one editable component")) {
        goto cleanup;
    }

    kain_ui_runtime_test_fill_state(state, bundle);
    if (!test_true(state->loaded, "runtime state should be loaded")) {
        goto cleanup;
    }
    if (!test_true(state->component_count == 3, "runtime state should build component records")) {
        goto cleanup;
    }
    if (!test_true(kain_ui_runtime_has_capability(state, KAIN_UI_RUNTIME_CAP_OVERLAY_COMPAT), "state should report overlay compatibility")) {
        goto cleanup;
    }
    if (!test_true(kain_ui_runtime_has_capability(state, KAIN_UI_RUNTIME_CAP_FOCUS_ROUTING), "state should report focus routing")) {
        goto cleanup;
    }
    if (!test_true(kain_ui_runtime_has_capability(state, KAIN_UI_RUNTIME_CAP_EDITABLE_CONTROLS), "state should report editable controls")) {
        goto cleanup;
    }

    editable = kain_ui_runtime_find_first_editable(state);
    if (!test_true(editable != NULL, "editable component should exist")) {
        goto cleanup;
    }
    if (!test_true(editable->id == 2ull, "editable component id should be stable")) {
        goto cleanup;
    }
    if (!test_true(strcmp(editable->value, "Ada") == 0, "editable component value should mirror source text")) {
        goto cleanup;
    }

    viewport = kain_ui_runtime_find_first_kind(state, KAIN_UI_COMPILED_NODE_VIEWPORT3D);
    if (!test_true(viewport != NULL, "viewport node should be discoverable")) {
        goto cleanup;
    }
    if (!test_true(viewport->id == 3ull, "viewport node should have expected id")) {
        goto cleanup;
    }

    if (!test_true(kain_ui_runtime_find_component(state, 1ull) != NULL, "root component should be findable by id")) {
        goto cleanup;
    }

    printf("[PASS] ui runtime bundle smoke\n");
    printf("summary: %s\n", report->summary);

cleanup:
    free(bundle);
    free(report);
    free(state);
    return g_failed ? 1 : 0;
}
