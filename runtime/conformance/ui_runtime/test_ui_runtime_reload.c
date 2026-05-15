#include "../../native/include/kain_ui_hot_reload.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int g_failed = 0;

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

static void fill_bundle_v1(KainUiCompiledBundle* bundle) {
    kain_ui_compiled_bundle_init(bundle);
    bundle->loaded = 1;
    bundle->has_root_id = 1;
    bundle->root_id = 1ull;
    snprintf(bundle->window_title, sizeof(bundle->window_title), "%s", "Reload V1");
    bundle->node_count = 3;

    bundle->nodes[0].id = 1ull;
    bundle->nodes[0].kind = KAIN_UI_COMPILED_NODE_PANEL;
    snprintf(bundle->nodes[0].title, sizeof(bundle->nodes[0].title), "%s", "Root");
    snprintf(bundle->nodes[0].tag, sizeof(bundle->nodes[0].tag), "%s", "panel");
    snprintf(bundle->nodes[0].persistent_layout_id, sizeof(bundle->nodes[0].persistent_layout_id), "%s", "root");
    bundle->nodes[0].child_count = 2;

    bundle->nodes[1].id = 2ull;
    bundle->nodes[1].parent_id = 1ull;
    bundle->nodes[1].has_parent = 1;
    bundle->nodes[1].depth = 1u;
    bundle->nodes[1].kind = KAIN_UI_COMPILED_NODE_ELEMENT;
    snprintf(bundle->nodes[1].title, sizeof(bundle->nodes[1].title), "%s", "Name");
    snprintf(bundle->nodes[1].text, sizeof(bundle->nodes[1].text), "%s", "Ada");
    snprintf(bundle->nodes[1].tag, sizeof(bundle->nodes[1].tag), "%s", "input");
    snprintf(bundle->nodes[1].layout_kind, sizeof(bundle->nodes[1].layout_kind), "%s", "text-entry");
    snprintf(bundle->nodes[1].persistent_layout_id, sizeof(bundle->nodes[1].persistent_layout_id), "%s", "name-field");

    bundle->nodes[2].id = 3ull;
    bundle->nodes[2].parent_id = 1ull;
    bundle->nodes[2].has_parent = 1;
    bundle->nodes[2].depth = 1u;
    bundle->nodes[2].kind = KAIN_UI_COMPILED_NODE_VIEWPORT3D;
    snprintf(bundle->nodes[2].title, sizeof(bundle->nodes[2].title), "%s", "Viewport");
    snprintf(bundle->nodes[2].tag, sizeof(bundle->nodes[2].tag), "%s", "viewport");
    snprintf(bundle->nodes[2].scene, sizeof(bundle->nodes[2].scene), "%s", "reload-scene");
    snprintf(bundle->nodes[2].persistent_layout_id, sizeof(bundle->nodes[2].persistent_layout_id), "%s", "viewport-main");
}

static void fill_bundle_v2(KainUiCompiledBundle* bundle) {
    kain_ui_compiled_bundle_init(bundle);
    bundle->loaded = 1;
    bundle->has_root_id = 1;
    bundle->root_id = 101ull;
    snprintf(bundle->window_title, sizeof(bundle->window_title), "%s", "Reload V2");
    bundle->node_count = 3;

    bundle->nodes[0].id = 101ull;
    bundle->nodes[0].kind = KAIN_UI_COMPILED_NODE_PANEL;
    snprintf(bundle->nodes[0].title, sizeof(bundle->nodes[0].title), "%s", "Root");
    snprintf(bundle->nodes[0].tag, sizeof(bundle->nodes[0].tag), "%s", "panel");
    snprintf(bundle->nodes[0].persistent_layout_id, sizeof(bundle->nodes[0].persistent_layout_id), "%s", "root");
    bundle->nodes[0].child_count = 2;

    bundle->nodes[1].id = 22ull;
    bundle->nodes[1].parent_id = 101ull;
    bundle->nodes[1].has_parent = 1;
    bundle->nodes[1].depth = 1u;
    bundle->nodes[1].kind = KAIN_UI_COMPILED_NODE_ELEMENT;
    snprintf(bundle->nodes[1].title, sizeof(bundle->nodes[1].title), "%s", "Name");
    snprintf(bundle->nodes[1].text, sizeof(bundle->nodes[1].text), "%s", "Grace");
    snprintf(bundle->nodes[1].tag, sizeof(bundle->nodes[1].tag), "%s", "input");
    snprintf(bundle->nodes[1].layout_kind, sizeof(bundle->nodes[1].layout_kind), "%s", "text-entry");
    snprintf(bundle->nodes[1].persistent_layout_id, sizeof(bundle->nodes[1].persistent_layout_id), "%s", "name-field");

    bundle->nodes[2].id = 303ull;
    bundle->nodes[2].parent_id = 101ull;
    bundle->nodes[2].has_parent = 1;
    bundle->nodes[2].depth = 1u;
    bundle->nodes[2].kind = KAIN_UI_COMPILED_NODE_VIEWPORT3D;
    snprintf(bundle->nodes[2].title, sizeof(bundle->nodes[2].title), "%s", "Viewport");
    snprintf(bundle->nodes[2].tag, sizeof(bundle->nodes[2].tag), "%s", "viewport");
    snprintf(bundle->nodes[2].scene, sizeof(bundle->nodes[2].scene), "%s", "reload-scene");
    snprintf(bundle->nodes[2].persistent_layout_id, sizeof(bundle->nodes[2].persistent_layout_id), "%s", "viewport-main");
}

static int test_reload_preserves_component_state(void) {
    KainUiCompiledBundle bundle_v1;
    KainUiCompiledBundle bundle_v2;
    KainUiRuntimeState state;
    KainUiRuntimeReloadOptions options;
    KainUiRuntimeReloadReport report;
    const KainUiRuntimeComponentState* value_component;

    fill_bundle_v1(&bundle_v1);
    fill_bundle_v2(&bundle_v2);
    kain_ui_runtime_state_init(&state);
    if (!test_true(kain_ui_runtime_state_load_bundle(&state, &bundle_v1), "initial bundle should load")) {
        return 0;
    }
    if (!test_true(kain_ui_runtime_request_focus(&state, 2ull), "editable component should accept focus")) {
        return 0;
    }

    snprintf(state.components[1].value, sizeof(state.components[1].value), "%s", "Ada Lovelace");
    state.components[1].value_length = strlen(state.components[1].value);
    state.components[1].cursor = state.components[1].value_length;
    state.components[1].dirty = 1;
    state.components[1].dirty_reason_mask = 7u;
    state.hovered_component_id = 3ull;

    kain_ui_runtime_reload_options_init(&options);
    kain_ui_runtime_reload_report_init(&report);
    if (!test_true(
            kain_ui_runtime_reload_bundle(&state, &bundle_v2, &options, &report),
            "reload should apply"
        )) {
        return 0;
    }
    if (!test_true(report.applied, "reload report should mark applied")) {
        return 0;
    }
    if (!test_true(report.preserved_focus, "focus should survive reload")) {
        return 0;
    }
    if (!test_true(report.preserved_active_edit_component, "active edit component should survive reload")) {
        return 0;
    }
    if (!test_true(report.preserved_hovered_component, "hovered component should survive reload")) {
        return 0;
    }
    if (!test_true(state.focused_component_id == 22ull, "focus should move to the new stable component id")) {
        return 0;
    }
    if (!test_true(state.active_edit_component_id == 22ull, "active edit id should move to the new stable component id")) {
        return 0;
    }
    if (!test_true(state.hovered_component_id == 303ull, "hovered id should move to the new stable component id")) {
        return 0;
    }

    value_component = kain_ui_runtime_find_component(&state, 22ull);
    if (!test_true(value_component != NULL, "reloaded editable component should exist")) {
        return 0;
    }
    if (!test_true(strcmp(value_component->persistent_layout_id, "name-field") == 0, "stable layout id should be retained")) {
        return 0;
    }
    if (!test_true(strcmp(value_component->value, "Ada Lovelace") == 0, "editable value should transfer")) {
        return 0;
    }
    if (!test_true(value_component->dirty, "dirty state should transfer")) {
        return 0;
    }
    if (!test_true(value_component->dirty_reason_mask == 7u, "dirty reason mask should transfer")) {
        return 0;
    }
    return 1;
}

static int test_shared_reload_channel_round_trip(void) {
    KainUiHotReloadChannel owner;
    KainUiHotReloadChannel client;
    char channel_name[KAIN_UI_HOT_RELOAD_CHANNEL_NAME_MAX];
    int request_generation;

    snprintf(
        channel_name,
        sizeof(channel_name),
        "kain-ui-reload.test.%llu",
        (unsigned long long)time(NULL)
    );

    if (!test_true(kain_ui_hot_reload_channel_create(&owner, channel_name), "channel owner should create mapping")) {
        return 0;
    }
    if (!test_true(kain_ui_hot_reload_channel_open(&client, channel_name), "channel client should open mapping")) {
        kain_ui_hot_reload_channel_close(&owner);
        return 0;
    }

    request_generation = kain_ui_hot_reload_channel_request_bundle(
        &client,
        "fixture.runtime_ui_bundle.json",
        UINT64_C(0xABCD1234),
        0
    );
    if (!test_true(request_generation > 0, "channel request should allocate a generation")) {
        kain_ui_hot_reload_channel_close(&client);
        kain_ui_hot_reload_channel_close(&owner);
        return 0;
    }
    if (!test_true(owner.control != NULL, "owner should expose shared control")) {
        kain_ui_hot_reload_channel_close(&client);
        kain_ui_hot_reload_channel_close(&owner);
        return 0;
    }
    if (!test_true(owner.control->request_generation == request_generation, "owner should observe requested generation")) {
        kain_ui_hot_reload_channel_close(&client);
        kain_ui_hot_reload_channel_close(&owner);
        return 0;
    }
    if (!test_true(
            strcmp(owner.control->requested_bundle_path, "fixture.runtime_ui_bundle.json") == 0,
            "owner should observe requested bundle path"
        )) {
        kain_ui_hot_reload_channel_close(&client);
        kain_ui_hot_reload_channel_close(&owner);
        return 0;
    }
    if (!test_true(owner.control->requested_fingerprint == UINT64_C(0xABCD1234), "owner should observe requested fingerprint")) {
        kain_ui_hot_reload_channel_close(&client);
        kain_ui_hot_reload_channel_close(&owner);
        return 0;
    }

    kain_ui_hot_reload_channel_close(&client);
    kain_ui_hot_reload_channel_close(&owner);
    return 1;
}

int main(void) {
    if (!test_reload_preserves_component_state()) {
        return 1;
    }
    if (!test_shared_reload_channel_round_trip()) {
        return 1;
    }

    printf("[PASS] ui runtime reload\n");
    return g_failed ? 1 : 0;
}
