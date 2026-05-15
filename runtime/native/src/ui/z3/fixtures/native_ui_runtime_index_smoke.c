#include "kain_native_ui_system.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct KainNativeUiSession KainNativeUiSession;

char* string_new(char* src) {
    return src;
}

double kain_clampd(double value, double min_value, double max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

#ifndef KAIN_NATIVE_UI_RUNTIME_SMOKE_USE_REAL_WIN32_GL
int64_t kain_native_ui_win32_gl_attach(KainNativeUiSession* session, const char* backend_id) {
    (void)session;
    (void)backend_id;
    return KAIN_NATIVE_UI_INVALID_ARGUMENT;
}

int64_t kain_native_ui_win32_gl_pump(KainNativeUiSession* session) {
    (void)session;
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_win32_gl_present(KainNativeUiSession* session) {
    (void)session;
    return KAIN_NATIVE_UI_OK;
}

void kain_native_ui_win32_gl_shutdown(KainNativeUiSession* session) {
    (void)session;
}

int kain_native_ui_win32_gl_clipboard_set_text(KainNativeUiSession* session, const char* text) {
    (void)session;
    (void)text;
    return 0;
}

int kain_native_ui_win32_gl_clipboard_get_text(
    KainNativeUiSession* session,
    char* out_text,
    size_t out_text_cap
) {
    (void)session;
    if (out_text && out_text_cap > 0u) {
        out_text[0] = '\0';
    }
    return 0;
}
#endif

static int require_true(int condition, const char* label) {
    if (!condition) {
        fprintf(stderr, "native_ui_runtime_index_smoke failed: %s\n", label);
        return 0;
    }
    return 1;
}

int main(void) {
    const char* backend = getenv("KAIN_NATIVE_UI_RUNTIME_SMOKE_BACKEND");
    int64_t session;
    int64_t font;
    int64_t texture;
    int64_t canvas;
    int64_t shader;
    int64_t root;
    int64_t command;
    int64_t menu;
    int64_t dialog;
    int64_t stable_lookup;
    int64_t cycle_status;
    int64_t response;
    int64_t host_presented;
    int64_t host_draw_count;
    int64_t hot_reload_generation;
    int64_t hot_reload_commit;
    int loop_index;
    double label_width;

    if (!require_true(kain_native_ui_reset() == KAIN_NATIVE_UI_OK, "reset")) {
        return 1;
    }

    session = kain_native_ui_session_create("native-ui-runtime-index-smoke", 640, 360);
    if (!require_true(session > 0, "session create")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_window_open(session, "Native UI Runtime Index Smoke", 640, 360) == KAIN_NATIVE_UI_OK,
            "window open")) {
        return 1;
    }
    if (!backend || !backend[0]) {
        backend = "software";
    }
    if (!require_true(kain_native_ui_host_attach(session, backend) == KAIN_NATIVE_UI_OK, "host attach")) {
        return 1;
    }
    hot_reload_generation = kain_native_ui_hot_reload_begin(session, "runtime-index-smoke.rev-a");
    if (!require_true(hot_reload_generation == 1, "hot reload begin")) {
        return 1;
    }
    if (!require_true(kain_native_ui_begin_frame(session, 16.0) == 1, "begin frame")) {
        return 1;
    }

    font = kain_native_ui_font_create(session, "font.body", "Inter", 14.0);
    texture = kain_native_ui_texture_create(session, "texture.command.icon", 1, 1, "rgba8", 4);
    canvas = kain_native_ui_canvas_create(session, "canvas.viewport", 320, 180);
    shader = kain_native_ui_shader_create(session, "shader.command.fill", "fragment", 64);
    if (!require_true(font > 0 && texture > 0 && canvas > 0 && shader > 0, "resource create")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_resource_set_bytes_hex(session, texture, "FF8F3FFF") == KAIN_NATIVE_UI_OK,
            "resource bytes")) {
        return 1;
    }
    if (!require_true(kain_native_ui_resource_count(session) == 4, "resource count")) {
        return 1;
    }

    root = kain_native_ui_node_create(session, "app.root");
    command = kain_native_ui_node_create(session, "author.command");
    if (!require_true(root > 0 && command > 0, "node create")) {
        return 1;
    }
    if (!require_true(kain_native_ui_node_set_stable_key(session, root, "root") == KAIN_NATIVE_UI_OK, "root key")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_node_set_stable_key(session, command, "command.launch") == KAIN_NATIVE_UI_OK,
            "command key")) {
        return 1;
    }
    if (!require_true(kain_native_ui_node_set_parent(session, command, root) == KAIN_NATIVE_UI_OK, "set parent")) {
        return 1;
    }
    cycle_status = kain_native_ui_node_set_parent(session, root, command);
    if (!require_true(cycle_status == KAIN_NATIVE_UI_INVALID_ARGUMENT, "cycle rejection")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_node_set_rect(session, root, 0.0, 0.0, 640.0, 360.0) == KAIN_NATIVE_UI_OK,
            "root rect")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_node_set_rect(session, command, 16.0, 16.0, 220.0, 48.0) == KAIN_NATIVE_UI_OK,
            "command rect")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_node_set_text(session, command, "Launch") == KAIN_NATIVE_UI_OK,
            "command text")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_node_set_style_string(session, command, "fill", "#21d4a1") == KAIN_NATIVE_UI_OK,
            "style set")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_node_set_state_string(session, command, "mode", "armed") == KAIN_NATIVE_UI_OK,
            "state set")) {
        return 1;
    }
    if (!require_true(kain_native_ui_focus(session, command) == KAIN_NATIVE_UI_OK, "focus")) {
        return 1;
    }
    if (!require_true(kain_native_ui_focused_node(session) == command, "focused node")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_drag_begin(session, command, "payload.launch", 24.0, 24.0) == KAIN_NATIVE_UI_OK,
            "drag begin")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_drag_update(session, 28.0, 28.0, root) == KAIN_NATIVE_UI_OK,
            "drag update")) {
        return 1;
    }
    if (!require_true(kain_native_ui_drag_drop_target(session) == root, "drag target")) {
        return 1;
    }

    menu = kain_native_ui_menu_create(session, "menu.command");
    if (!require_true(menu > 0, "menu create")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_menu_add_item(session, menu, "launch", "Launch", 101) > 0,
            "menu item create")) {
        return 1;
    }
    if (!require_true(kain_native_ui_menu_open(session, menu, 16.0, 68.0) == KAIN_NATIVE_UI_OK, "menu open")) {
        return 1;
    }
    if (!require_true(kain_native_ui_menu_active(session) == menu, "menu active")) {
        return 1;
    }

    dialog = kain_native_ui_dialog_request(session, "confirm", "Launch", "Proceed");
    if (!require_true(dialog > 0, "dialog create")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_dialog_respond(session, dialog, 7, "ok") == KAIN_NATIVE_UI_OK,
            "dialog respond")) {
        return 1;
    }
    response = kain_native_ui_dialog_poll_response(session);
    if (!require_true(response == 7, "dialog response")) {
        return 1;
    }

    label_width = kain_native_ui_text_measure_width(session, font, "Launch");
    if (!require_true(label_width > 0.0, "text measure")) {
        return 1;
    }

    if (!require_true(
            kain_native_ui_draw_rect(session, root, 0.0, 0.0, 640.0, 360.0, "root.bg") == 1,
            "draw root")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_draw_rect(session, command, 16.0, 16.0, 220.0, 48.0, "command.fill") == 2,
            "draw command")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_draw_resource(session, command, texture, 184.0, 24.0, 24.0, 24.0, "command.icon") == 3,
            "draw texture")) {
        return 1;
    }
    if (!require_true(
            kain_native_ui_draw_text(session, command, font, 28.0, 42.0, "Launch", "command.label") == 4,
            "draw text")) {
        return 1;
    }
    if (!require_true(kain_native_ui_end_frame(session) == 4, "end frame")) {
        return 1;
    }

    stable_lookup = kain_native_ui_node_find_by_stable_key(session, "command.launch");
    if (!require_true(stable_lookup == command, "stable key lookup")) {
        return 1;
    }
    if (!require_true(kain_native_ui_hit_test(session, 24.0, 24.0) == command, "hit test")) {
        return 1;
    }

    host_presented = kain_native_ui_host_present(session);
    host_draw_count = kain_native_ui_host_presented_draw_count(session);
    if (!require_true(host_presented == 4, "host present")) {
        return 1;
    }
    if (!require_true(host_draw_count == 4, "host draw count")) {
        return 1;
    }
    if (!require_true(kain_native_ui_host_frame_hash(session) > 0, "host frame hash")) {
        return 1;
    }
    hot_reload_commit = kain_native_ui_hot_reload_commit(session);
    if (!require_true(hot_reload_commit == 1, "hot reload commit")) {
        return 1;
    }

    if (!require_true(kain_native_ui_node_destroy(session, command) == KAIN_NATIVE_UI_OK, "node destroy")) {
        return 1;
    }
    if (!require_true(kain_native_ui_node_find_by_stable_key(session, "command.launch") == 0, "destroyed key cleared")) {
        return 1;
    }
    if (!require_true(kain_native_ui_focused_node(session) == 0, "destroy clears focus")) {
        return 1;
    }
    if (!require_true(kain_native_ui_drag_drop_target(session) == 0, "destroy clears drag target")) {
        return 1;
    }

    for (loop_index = 0; loop_index < 256; loop_index += 1) {
        char stable_key[64];
        int64_t loop_node = kain_native_ui_node_create(session, "loop.node");
        if (!require_true(loop_node > 0, "loop node create")) {
            return 1;
        }
        snprintf(stable_key, sizeof(stable_key), "loop.node.%d", loop_index);
        if (!require_true(
                kain_native_ui_node_set_stable_key(session, loop_node, stable_key) == KAIN_NATIVE_UI_OK,
                "loop stable key")) {
            return 1;
        }
        if (!require_true(
                kain_native_ui_node_set_style_string(session, loop_node, "fill", "#010203") == KAIN_NATIVE_UI_OK,
                "loop style")) {
            return 1;
        }
        if (!require_true(
                kain_native_ui_node_set_state_string(session, loop_node, "phase", "loop") == KAIN_NATIVE_UI_OK,
                "loop state")) {
            return 1;
        }
        if (!require_true(kain_native_ui_node_destroy(session, loop_node) == KAIN_NATIVE_UI_OK, "loop destroy")) {
            return 1;
        }
    }

    if (!require_true(kain_native_ui_node_count(session) == 1, "final node count")) {
        return 1;
    }
    if (!require_true(kain_native_ui_session_destroy(session) == KAIN_NATIVE_UI_OK, "session destroy")) {
        return 1;
    }
    if (!require_true(kain_native_ui_session_count() == 0, "session count")) {
        return 1;
    }

    return 0;
}
