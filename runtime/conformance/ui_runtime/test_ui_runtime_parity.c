#include "../../native/include/ui_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* kUiParityFixtureDefault =
    "fixtures/ui_runtime_parity_bundle.json";

static int g_failed = 0;

static const char* parity_fixture_path(void) {
    const char* override = getenv("KAIN_UI_PARITY_FIXTURE");
    if (override && override[0]) {
        return override;
    }
    return kUiParityFixtureDefault;
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

static int test_root_and_nodes_are_canonical(
    const KainUiCompiledBundle* compiled_bundle,
    const KainUiRuntimeState* runtime_state
) {
    if (!test_true(compiled_bundle->has_root_id, "compiled bundle should expose a root id")) {
        return 0;
    }
    if (!test_true(compiled_bundle->root_id == 1ull, "compiled bundle root id should match canonical output.tree root")) {
        return 0;
    }
    if (!test_true(runtime_state->bundle.root_id == compiled_bundle->root_id, "runtime state bundle root should match compiled bundle")) {
        return 0;
    }
    if (!test_true(compiled_bundle->node_count == 3, "compiled bundle should expose three canonical output.tree nodes")) {
        return 0;
    }
    if (!test_true(runtime_state->component_count == compiled_bundle->node_count, "runtime component count should match canonical node count")) {
        return 0;
    }
    if (!test_true(compiled_bundle->nodes[0].id == compiled_bundle->root_id, "canonical root node should be first compiled node")) {
        return 0;
    }
    if (!test_true(compiled_bundle->nodes[0].kind == KAIN_UI_COMPILED_NODE_PANEL, "canonical root node should be a panel")) {
        return 0;
    }
    if (!test_true(compiled_bundle->nodes[1].kind == KAIN_UI_COMPILED_NODE_ELEMENT, "second canonical node should be an element")) {
        return 0;
    }
    if (!test_true(compiled_bundle->nodes[2].kind == KAIN_UI_COMPILED_NODE_VIEWPORT3D, "third canonical node should be a viewport3d")) {
        return 0;
    }
    if (!test_true(strcmp(compiled_bundle->nodes[0].title, "Root Panel") == 0, "canonical root node should keep its authored title")) {
        return 0;
    }
    if (!test_true(strcmp(compiled_bundle->nodes[2].scene, "geometry_fixture") == 0, "canonical viewport node should keep its authored scene")) {
        return 0;
    }
    return 1;
}

int main(void) {
    const char* fixture_path = parity_fixture_path();
    KainUiCompiledBundle* compiled_bundle;
    KainUiRuntimeState* runtime_state;
    const KainUiRuntimeComponentState* runtime_panel;
    const KainUiRuntimeComponentState* runtime_viewport;
    const KainUiCompiledNode* compiled_panel;
    const KainUiCompiledNode* compiled_viewport;

    compiled_bundle = (KainUiCompiledBundle*)calloc(1, sizeof(*compiled_bundle));
    runtime_state = (KainUiRuntimeState*)calloc(1, sizeof(*runtime_state));
    if (!compiled_bundle || !runtime_state) {
        fprintf(stderr, "[FAIL] allocation failed\n");
        fflush(stderr);
        free(compiled_bundle);
        free(runtime_state);
        return 1;
    }

    if (!test_true(kain_ui_compiled_bundle_load_from_path(fixture_path, compiled_bundle), "compiled bundle should load from shared fixture")) {
        goto cleanup;
    }
    if (!test_true(kain_ui_runtime_state_load_from_path(runtime_state, fixture_path), "runtime state should load from shared fixture")) {
        goto cleanup;
    }

    if (!test_true(compiled_bundle->loaded, "compiled bundle should report loaded")) {
        goto cleanup;
    }
    if (!test_true(runtime_state->loaded, "runtime state should report loaded")) {
        goto cleanup;
    }
    if (!test_root_and_nodes_are_canonical(compiled_bundle, runtime_state)) {
        goto cleanup;
    }

    compiled_panel = kain_ui_compiled_bundle_find_first_kind(compiled_bundle, KAIN_UI_COMPILED_NODE_PANEL);
    if (!test_true(compiled_panel != NULL, "compiled bundle should expose a panel node")) {
        goto cleanup;
    }
    compiled_viewport = kain_ui_compiled_bundle_find_first_kind(compiled_bundle, KAIN_UI_COMPILED_NODE_VIEWPORT3D);
    if (!test_true(compiled_viewport != NULL, "compiled bundle should expose a viewport3d node")) {
        goto cleanup;
    }

    runtime_panel = kain_ui_runtime_find_first_kind(runtime_state, KAIN_UI_COMPILED_NODE_PANEL);
    if (!test_true(runtime_panel != NULL, "runtime state should expose a panel record")) {
        goto cleanup;
    }
    runtime_viewport = kain_ui_runtime_find_first_kind(runtime_state, KAIN_UI_COMPILED_NODE_VIEWPORT3D);
    if (!test_true(runtime_viewport != NULL, "runtime state should expose a viewport record")) {
        goto cleanup;
    }

    if (!test_true(runtime_panel->id == compiled_panel->id, "runtime panel id should match compiled bundle node")) {
        goto cleanup;
    }
    if (!test_true(runtime_viewport->id == compiled_viewport->id, "runtime viewport id should match compiled bundle node")) {
        goto cleanup;
    }
    if (!test_true(runtime_state->validation.valid, "runtime validation should be valid")) {
        goto cleanup;
    }
    if (!test_true(runtime_state->validation.component_count == compiled_bundle->node_count, "runtime validation component count should match canonical output.tree nodes")) {
        goto cleanup;
    }
    if (!test_true(runtime_state->validation.overlay_compatible, "runtime validation should keep overlay compatibility")) {
        goto cleanup;
    }

    printf("[PASS] ui runtime parity smoke\n");

cleanup:
    free(compiled_bundle);
    free(runtime_state);
    return g_failed ? 1 : 0;
}
