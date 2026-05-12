#include "../../native/include/kain_native_ui_system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

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
    kain_native_ui_node_set_state_string(session, command, "shape.kind", "tetra.surface");
    kain_native_ui_node_set_state_string(session, command, "hit.kind", "authored.math");
    if (!test_true(kain_native_ui_node_find_by_stable_key(session, "command.launch") == command, "stable key lookup should preserve hot reload identity")) {
        return 1;
    }
    if (!test_true(strcmp(kain_native_ui_node_state_string(session, command, "shape.kind", "rect"), "tetra.surface") == 0, "state should survive stable-key reconciliation identity")) {
        return 1;
    }
    if (!test_true(kain_native_ui_state_count(session) == 2, "host services should track authored state cells")) {
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
    texture = kain_native_ui_texture_create(session, "texture.icon", 2, 2, "rgba8", 16);
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
    if (!test_true(kain_native_ui_resource_width(session, texture) == 2, "texture width should round trip")) {
        return 1;
    }
    if (!test_true(
            kain_native_ui_resource_set_bytes_hex(session, texture, "FF8F3FFF7DC9FFFF1F242EFFEEF2F8FF") == KAIN_NATIVE_UI_OK,
            "resource hex upload should succeed"
        )) {
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
    kain_native_ui_draw_text(session, command, font, 28.0, 38.0, "Launch", "command.label");
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
    if (!test_true(kain_native_ui_draw_command_font(session, 1) == font, "text draw command should expose font handle")) {
        return 1;
    }

#ifdef _WIN32
    {
        const char* screenshot_path = "bin/win32_gl_host_services.bmp";
        int64_t live_session;
        int64_t live_root;
        int64_t live_command;
        int64_t live_font;
        int64_t live_texture;
        int64_t live_menu;
        int64_t live_dialog;
        HWND live_hwnd = NULL;
        char live_class_name[96];
        int frame_index;
        int saw_pointer_down = 0;
        int saw_key_down = 0;
        int saw_text_input = 0;
        int saw_menu_command = 0;
        int saw_dialog_response = 0;

        remove(screenshot_path);
        _putenv_s("KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH", screenshot_path);
        _putenv_s("KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_AFTER_FRAMES", "3");
        _putenv_s("KAIN_NATIVE_UI_WIN32_GL_MENU_AUTO_COMMAND", "101");
        _putenv_s("KAIN_NATIVE_UI_WIN32_GL_DIALOG_AUTO_RESPONSE", "9");

        live_session = kain_native_ui_session_create("raw-native-ui-live", 640, 360);
        if (!test_true(live_session > 0, "live session handle should be positive")) {
            return 1;
        }
        if (!test_true(kain_native_ui_window_open(live_session, "Host Services Live", 640, 360) == KAIN_NATIVE_UI_OK, "live window open should succeed")) {
            return 1;
        }
        if (!test_true(kain_native_ui_host_attach(live_session, "win32-gl") == KAIN_NATIVE_UI_OK, "live host attach should succeed")) {
            return 1;
        }
        snprintf(live_class_name, sizeof(live_class_name), "KainNativeUiWin32GlWindow.%lld", (long long)live_session);
        live_root = kain_native_ui_node_create(live_session, "author.root");
        live_command = kain_native_ui_node_create(live_session, "author.command");
        live_font = kain_native_ui_font_create(live_session, "font.body.live", "Inter", 16.0);
        live_texture = kain_native_ui_texture_create(live_session, "texture.live.icon", 2, 2, "rgba8", 16);
        live_menu = kain_native_ui_menu_create(live_session, "menu.live");
        live_dialog = kain_native_ui_dialog_request(live_session, "confirm", "Launch", "Proceed");

        kain_native_ui_node_set_parent(live_session, live_command, live_root);
        kain_native_ui_node_set_rect(live_session, live_root, 0.0, 0.0, 640.0, 360.0);
        kain_native_ui_node_set_rect(live_session, live_command, 16.0, 16.0, 180.0, 36.0);
        kain_native_ui_node_set_state_string(live_session, live_command, "shape.kind", "capsule.command");
        kain_native_ui_node_set_state_string(live_session, live_command, "draw.kind", "shader.resource");
        kain_native_ui_resource_set_bytes_hex(live_session, live_texture, "FF8F3FFF7DC9FFFF1F242EFFEEF2F8FF");
        kain_native_ui_node_set_style_f64(live_session, live_root, "live.bg.color.r", 0.08);
        kain_native_ui_node_set_style_f64(live_session, live_root, "live.bg.color.g", 0.09);
        kain_native_ui_node_set_style_f64(live_session, live_root, "live.bg.color.b", 0.11);
        kain_native_ui_node_set_style_f64(live_session, live_root, "live.bg.color.a", 1.0);
        kain_native_ui_node_set_style_f64(live_session, live_command, "live.command.color.r", 0.21);
        kain_native_ui_node_set_style_f64(live_session, live_command, "live.command.color.g", 0.55);
        kain_native_ui_node_set_style_f64(live_session, live_command, "live.command.color.b", 0.46);
        kain_native_ui_node_set_style_f64(live_session, live_command, "live.command.color.a", 1.0);
        kain_native_ui_node_set_style_f64(live_session, live_command, "live.label.color.r", 0.97);
        kain_native_ui_node_set_style_f64(live_session, live_command, "live.label.color.g", 0.98);
        kain_native_ui_node_set_style_f64(live_session, live_command, "live.label.color.b", 1.0);
        kain_native_ui_node_set_style_f64(live_session, live_command, "live.label.color.a", 1.0);
        kain_native_ui_clipboard_set_text(live_session, "win32-live");
        if (!test_true(strcmp(kain_native_ui_clipboard_text(live_session), "win32-live") == 0, "live clipboard bridge should round trip")) {
            return 1;
        }
        if (!test_true(kain_native_ui_state_count(live_session) == 2, "live host should accept generic authored state cells")) {
            return 1;
        }
        kain_native_ui_menu_add_item(live_session, live_menu, "launch", "Launch", 101);
        kain_native_ui_menu_open(live_session, live_menu, 16.0, 56.0);
        if (!test_true(live_dialog > 0, "live dialog should be requested")) {
            return 1;
        }

        for (frame_index = 0; frame_index < 4; frame_index += 1) {
            kain_native_ui_begin_frame(live_session, 16.0);
            kain_native_ui_draw_rect(live_session, live_root, 0.0, 0.0, 640.0, 360.0, "live.bg");
            kain_native_ui_draw_rect(live_session, live_command, 16.0, 16.0, 180.0, 36.0, "live.command");
            kain_native_ui_draw_text(live_session, live_command, live_font, 28.0, 40.0, "Launch", "live.label");
            kain_native_ui_draw_resource(live_session, live_command, live_texture, 164.0, 18.0, 32.0, 32.0, "live.icon");
            kain_native_ui_end_frame(live_session);
            kain_native_ui_present(live_session);
            if (!test_true(kain_native_ui_host_present(live_session) == 4, "live host present should render draw commands")) {
                return 1;
            }
            if (frame_index == 0) {
                live_hwnd = FindWindowA(live_class_name, NULL);
                if (!live_hwnd) {
                    live_hwnd = FindWindowA(NULL, "Host Services Live");
                }
                if (!test_true(live_hwnd != NULL, "live host should create a real window")) {
                    return 1;
                }
                PostMessageA(live_hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(24, 24));
                PostMessageA(live_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(24, 24));
                PostMessageA(live_hwnd, WM_LBUTTONUP, 0, MAKELPARAM(24, 24));
                PostMessageA(live_hwnd, WM_KEYDOWN, 'A', 0);
                PostMessageA(live_hwnd, WM_CHAR, 'x', 0);
            }
            kain_native_ui_host_pump(live_session);
        }

        while (kain_native_ui_poll_event(live_session) == 1) {
            const char* kind = kain_native_ui_event_kind(live_session);
            if (strcmp(kind, "pointer.down") == 0) {
                saw_pointer_down = 1;
            } else if (strcmp(kind, "key.down") == 0) {
                saw_key_down = 1;
            } else if (strcmp(kind, "text.input") == 0) {
                saw_text_input = 1;
            } else if (strcmp(kind, "menu.command") == 0) {
                saw_menu_command = 1;
            } else if (strcmp(kind, "dialog.response") == 0) {
                saw_dialog_response = 1;
            }
        }

        if (!test_true(saw_pointer_down && saw_key_down && saw_text_input, "win32 input bridge should emit pointer and keyboard events")) {
            return 1;
        }
        if (!test_true(saw_menu_command && saw_dialog_response, "win32 desktop service bridge should emit menu and dialog events")) {
            return 1;
        }
        if (!test_true(kain_native_ui_host_should_close(live_session) == 1, "live host should request close after screenshot capture")) {
            return 1;
        }
        if (!test_true(GetFileAttributesA(screenshot_path) != INVALID_FILE_ATTRIBUTES, "live host should create a screenshot artifact")) {
            return 1;
        }

        kain_native_ui_session_destroy(live_session);
        _putenv_s("KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH", "");
        _putenv_s("KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_AFTER_FRAMES", "");
        _putenv_s("KAIN_NATIVE_UI_WIN32_GL_MENU_AUTO_COMMAND", "");
        _putenv_s("KAIN_NATIVE_UI_WIN32_GL_DIALOG_AUTO_RESPONSE", "");
    }
#endif

    if (!test_true(kain_native_ui_hot_reload_commit(session) == generation, "hot reload commit should preserve generation")) {
        return 1;
    }

    printf("[PASS] native ui host services smoke\n");
    return g_failed ? 1 : 0;
}
