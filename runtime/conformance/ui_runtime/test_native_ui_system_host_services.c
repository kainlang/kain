#include "../../native/include/kain_native_ui_system.h"

#include <stdio.h>
#include <string.h>

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

int main(void) {
    int64_t session;
    int64_t root;
    int64_t command;
    int64_t font;
    int64_t texture;
    int64_t canvas;
    int64_t shader;
    int64_t menu;
    int64_t dialog;
    int64_t generation;
    int64_t draw_count;

    if (!test_true(kain_native_ui_reset() == KAIN_NATIVE_UI_OK, "reset should succeed")) {
        return 1;
    }

    session = kain_native_ui_session_create("raw-native-ui-host-services", 800, 480);
    if (!test_true(session > 0, "session handle should be positive")) {
        return 1;
    }
    if (!test_true(kain_native_ui_window_open(session, "Host Services", 800, 480) == KAIN_NATIVE_UI_OK, "window open should succeed")) {
        return 1;
    }
    if (!test_true(kain_native_ui_host_attach(session, "software") == KAIN_NATIVE_UI_OK, "host attach should accept a backend id")) {
        return 1;
    }
    if (!test_true(strcmp(kain_native_ui_host_backend(session), "software") == 0, "host backend should round trip")) {
        return 1;
    }

    generation = kain_native_ui_hot_reload_begin(session, "rev-a");
    if (!test_true(generation == 1, "hot reload begin should advance generation")) {
        return 1;
    }
    if (!test_true(strcmp(kain_native_ui_hot_reload_key(session), "rev-a") == 0, "hot reload key should round trip")) {
        return 1;
    }

    root = kain_native_ui_node_create(session, "author.root");
    command = kain_native_ui_node_create(session, "author.command");
    if (!test_true(root > 0 && command > 0, "authored nodes should be created")) {
        return 1;
    }
    kain_native_ui_node_set_stable_key(session, root, "root");
    kain_native_ui_node_set_stable_key(session, command, "command.launch");
    kain_native_ui_node_set_parent(session, command, root);
    kain_native_ui_node_set_rect(session, root, 0.0, 0.0, 800.0, 480.0);
    kain_native_ui_node_set_rect(session, command, 16.0, 16.0, 180.0, 36.0);
    kain_native_ui_node_set_text(session, command, "Launch");
    if (!test_true(kain_native_ui_node_find_by_stable_key(session, "command.launch") == command, "stable key lookup should preserve hot reload identity")) {
        return 1;
    }

    if (!test_true(kain_native_ui_accessibility_set_role(session, command, "action") == KAIN_NATIVE_UI_OK, "accessibility role should write")) {
        return 1;
    }
    if (!test_true(kain_native_ui_accessibility_set_label(session, command, "Launch command") == KAIN_NATIVE_UI_OK, "accessibility label should write")) {
        return 1;
    }
    if (!test_true(strcmp(kain_native_ui_accessibility_role(session, command), "action") == 0, "accessibility role should round trip")) {
        return 1;
    }

    font = kain_native_ui_font_create(session, "font.body", "Inter", 14.0);
    texture = kain_native_ui_texture_create(session, "texture.icon", 32, 32, "rgba8", 4096);
    canvas = kain_native_ui_canvas_create(session, "canvas.main", 800, 480);
    shader = kain_native_ui_shader_create(session, "shader.fill", "fragment", 128);
    if (!test_true(font > 0 && texture > 0 && canvas > 0 && shader > 0, "resources should be generic typed handles")) {
        return 1;
    }
    if (!test_true(kain_native_ui_resource_count(session) == 4, "resource count should track handles")) {
        return 1;
    }
    if (!test_true(kain_native_ui_text_measure_width(session, font, "Launch") > 20.0, "text measurement should use font metadata")) {
        return 1;
    }
    if (!test_true(kain_native_ui_resource_width(session, texture) == 32, "texture width should round trip")) {
        return 1;
    }

    if (!test_true(kain_native_ui_clipboard_set_text(session, "copied") == KAIN_NATIVE_UI_OK, "clipboard write should succeed")) {
        return 1;
    }
    if (!test_true(strcmp(kain_native_ui_clipboard_text(session), "copied") == 0, "clipboard text should round trip")) {
        return 1;
    }
    if (!test_true(kain_native_ui_ime_begin(session, command) == KAIN_NATIVE_UI_OK, "IME begin should target a node")) {
        return 1;
    }
    kain_native_ui_ime_commit_text(session, "typed");
    if (!test_true(strcmp(kain_native_ui_ime_text(session), "typed") == 0, "IME text should round trip")) {
        return 1;
    }

    if (!test_true(kain_native_ui_drag_begin(session, command, "payload:launch", 20.0, 20.0) == KAIN_NATIVE_UI_OK, "drag begin should target node")) {
        return 1;
    }
    kain_native_ui_drag_update(session, 40.0, 44.0, root);
    if (!test_true(kain_native_ui_drag_drop_target(session) == root, "drag target should round trip")) {
        return 1;
    }
    if (!test_true(strcmp(kain_native_ui_drag_payload(session), "payload:launch") == 0, "drag payload should round trip")) {
        return 1;
    }

    menu = kain_native_ui_menu_create(session, "menu.command");
    if (!test_true(menu > 0, "menu should be an authored data handle")) {
        return 1;
    }
    kain_native_ui_menu_add_item(session, menu, "launch", "Launch", 101);
    kain_native_ui_menu_open(session, menu, 16.0, 52.0);
    if (!test_true(kain_native_ui_menu_active(session) == menu, "active menu should round trip")) {
        return 1;
    }
    if (!test_true(kain_native_ui_menu_item_command(session, menu, 0) == 101, "menu command should round trip")) {
        return 1;
    }

    dialog = kain_native_ui_dialog_request(session, "confirm", "Launch", "Proceed");
    if (!test_true(dialog > 0 && kain_native_ui_dialog_active(session) == dialog, "dialog request should become active")) {
        return 1;
    }
    kain_native_ui_dialog_respond(session, dialog, 7, "ok");
    if (!test_true(kain_native_ui_dialog_poll_response(session) == 7, "dialog response result should poll")) {
        return 1;
    }
    if (!test_true(strcmp(kain_native_ui_dialog_response_text(session), "ok") == 0, "dialog response text should round trip")) {
        return 1;
    }

    kain_native_ui_begin_frame(session, 16.0);
    kain_native_ui_draw_rect(session, command, 16.0, 16.0, 180.0, 36.0, "command.fill");
    kain_native_ui_draw_text(session, command, 28.0, 38.0, "Launch", "command.label");
    kain_native_ui_draw_resource(session, command, texture, 164.0, 18.0, 32.0, 32.0, "command.icon");
    draw_count = kain_native_ui_host_present(session);
    if (!test_true(draw_count == 3, "host present should consume draw commands")) {
        return 1;
    }
    if (!test_true(kain_native_ui_host_presented_draw_count(session) == 3, "host draw count should round trip")) {
        return 1;
    }
    if (!test_true(kain_native_ui_host_frame_hash(session) > 0, "host frame hash should be stable nonzero metadata")) {
        return 1;
    }
    if (!test_true(kain_native_ui_draw_command_resource(session, 2) == texture, "resource draw command should expose handle")) {
        return 1;
    }

    if (!test_true(kain_native_ui_hot_reload_commit(session) == generation, "hot reload commit should preserve generation")) {
        return 1;
    }

    printf("[PASS] native ui host services smoke\n");
    return g_failed ? 1 : 0;
}
