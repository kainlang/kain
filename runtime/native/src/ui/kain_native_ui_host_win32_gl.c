#include "kain_native_ui_host_adapter.h"

#ifdef _WIN32

#include "kain_runtime_win32.h"

#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KAIN_NATIVE_UI_WIN32_GL_CLASS_NAME "KainNativeUiWin32GlWindow"
#define KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_ENV "KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_PATH"
#define KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_ENV "KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_AFTER_FRAMES"
#define KAIN_NATIVE_UI_WIN32_GL_MENU_AUTO_ENV "KAIN_NATIVE_UI_WIN32_GL_MENU_AUTO_COMMAND"
#define KAIN_NATIVE_UI_WIN32_GL_DIALOG_AUTO_ENV "KAIN_NATIVE_UI_WIN32_GL_DIALOG_AUTO_RESPONSE"

typedef struct KainNativeUiWin32GlTextureCacheEntry {
    int in_use;
    int64_t resource_id;
    uint64_t uploaded_revision;
    GLuint texture_id;
} KainNativeUiWin32GlTextureCacheEntry;

typedef struct KainNativeUiWin32GlState {
    KainNativeUiSession* session;
    HINSTANCE instance;
    HWND hwnd;
    KainWin32GlSurface surface;
    int class_registered;
    int window_created;
    int frame_counter;
    int screenshot_written;
    int auto_exit_after_frames;
    int menu_auto_command;
    int dialog_auto_response;
    int last_pointer_x;
    int last_pointer_y;
    char class_name[64];
    char screenshot_path[512];
    KainNativeUiWin32GlTextureCacheEntry textures[KAIN_NATIVE_UI_MAX_RESOURCES];
} KainNativeUiWin32GlState;

static int kain_native_ui_win32_gl_get_x(LPARAM l_param) {
    return (int)(short)LOWORD(l_param);
}

static int kain_native_ui_win32_gl_get_y(LPARAM l_param) {
    return (int)(short)HIWORD(l_param);
}

static void kain_native_ui_win32_gl_copy_text(char* out_text, size_t out_text_cap, const char* text) {
    if (!out_text || out_text_cap == 0) {
        return;
    }
    if (!text) {
        text = "";
    }
    snprintf(out_text, out_text_cap, "%s", text);
}

static KainNativeUiMenu* kain_native_ui_win32_gl_find_menu(
    KainNativeUiSession* session,
    int64_t menu_id
) {
    int64_t index;
    if (!session || menu_id <= 0) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_MENUS; index += 1) {
        if (session->menus[index].in_use && session->menus[index].id == menu_id) {
            return &session->menus[index];
        }
    }
    return NULL;
}

static KainNativeUiDialog* kain_native_ui_win32_gl_find_dialog(
    KainNativeUiSession* session,
    int64_t dialog_id
) {
    int64_t index;
    if (!session || dialog_id <= 0) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_DIALOGS; index += 1) {
        if (session->dialogs[index].in_use && session->dialogs[index].id == dialog_id) {
            return &session->dialogs[index];
        }
    }
    return NULL;
}

static KainNativeUiStyleRecord* kain_native_ui_win32_gl_find_style(
    KainNativeUiSession* session,
    int64_t node_id,
    const char* key
) {
    int64_t index;
    if (!session || !key || !key[0]) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_STYLES; index += 1) {
        if (session->styles[index].in_use &&
            session->styles[index].node_id == node_id &&
            strcmp(session->styles[index].key, key) == 0) {
            return &session->styles[index];
        }
    }
    return NULL;
}

static KainNativeUiResource* kain_native_ui_win32_gl_find_resource(
    KainNativeUiSession* session,
    int64_t resource_id
) {
    int64_t index;
    if (!session || resource_id <= 0) {
        return NULL;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_RESOURCES; index += 1) {
        if (session->resources[index].in_use && session->resources[index].id == resource_id) {
            return &session->resources[index];
        }
    }
    return NULL;
}

static float kain_native_ui_win32_gl_style_color_component(
    KainNativeUiSession* session,
    int64_t node_id,
    const char* style_key,
    const char* suffix,
    float fallback
) {
    char key[192];
    KainNativeUiStyleRecord* record;
    if (!style_key || !style_key[0]) {
        return fallback;
    }
    snprintf(key, sizeof(key), "%s.%s", style_key, suffix);
    record = kain_native_ui_win32_gl_find_style(session, node_id, key);
    if (!record) {
        return fallback;
    }
    if (record->value_kind == KAIN_NATIVE_UI_STYLE_F64) {
        return (float)record->f64_value;
    }
    if (record->value_kind == KAIN_NATIVE_UI_STYLE_I64) {
        return (float)record->i64_value;
    }
    return fallback;
}

static void kain_native_ui_win32_gl_style_fill_color(
    KainNativeUiSession* session,
    int64_t node_id,
    const char* style_key,
    float fallback_r,
    float fallback_g,
    float fallback_b,
    float fallback_a,
    float* out_r,
    float* out_g,
    float* out_b,
    float* out_a
) {
    *out_r = kain_native_ui_win32_gl_style_color_component(
        session,
        node_id,
        style_key,
        "color.r",
        fallback_r
    );
    *out_g = kain_native_ui_win32_gl_style_color_component(
        session,
        node_id,
        style_key,
        "color.g",
        fallback_g
    );
    *out_b = kain_native_ui_win32_gl_style_color_component(
        session,
        node_id,
        style_key,
        "color.b",
        fallback_b
    );
    *out_a = kain_native_ui_win32_gl_style_color_component(
        session,
        node_id,
        style_key,
        "color.a",
        fallback_a
    );
}

static int kain_native_ui_win32_gl_int_env(const char* name, int fallback) {
    char* value = kain_env_dup(name);
    int parsed = fallback;
    if (value && value[0]) {
        parsed = atoi(value);
    }
    kain_env_free(value);
    return parsed;
}

static void kain_native_ui_win32_gl_read_screenshot_env(
    char* out_path,
    size_t out_path_cap
) {
    char* value = kain_env_dup(KAIN_NATIVE_UI_WIN32_GL_SCREENSHOT_ENV);
    if (!value || !value[0]) {
        if (out_path && out_path_cap > 0) {
            out_path[0] = '\0';
        }
        kain_env_free(value);
        return;
    }
    kain_native_ui_win32_gl_copy_text(out_path, out_path_cap, value);
    kain_env_free(value);
}

static void kain_native_ui_win32_gl_push_event(
    KainNativeUiSession* session,
    const char* kind,
    int64_t target_node_id,
    double x,
    double y,
    int64_t key_code,
    const char* text
) {
    if (!session) {
        return;
    }
    kain_native_ui_push_event(session->id, kind, target_node_id, x, y, key_code, text);
}

static void kain_native_ui_win32_gl_begin_overlay(int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)width, (double)height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void kain_native_ui_win32_gl_end_overlay(void) {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

static void kain_native_ui_win32_gl_rebuild_font(
    KainWin32GlSurface* surface,
    int pixel_height
) {
    if (!surface || !surface->dc) {
        return;
    }
    if (surface->font_ready && surface->font_base != 0) {
        glDeleteLists(surface->font_base, 96);
        surface->font_base = 0;
        surface->font_ready = 0;
    }
    surface->font_pixel_height = pixel_height > 0 ? pixel_height : 16;
    kain_win32_gl_ensure_font(
        surface->dc,
        &surface->font_base,
        &surface->font_ready,
        surface->font_pixel_height
    );
}

static int kain_native_ui_win32_gl_font_pixel_height(
    KainNativeUiSession* session,
    int64_t font_resource_id
) {
    KainNativeUiResource* resource = kain_native_ui_win32_gl_find_resource(session, font_resource_id);
    if (!resource || resource->scalar_value <= 0.0) {
        return 16;
    }
    return (int)(resource->scalar_value + 0.5);
}

static KainNativeUiWin32GlTextureCacheEntry*
kain_native_ui_win32_gl_texture_entry(
    KainNativeUiWin32GlState* state,
    int64_t resource_id
) {
    int index;
    KainNativeUiWin32GlTextureCacheEntry* empty = NULL;
    for (index = 0; index < KAIN_NATIVE_UI_MAX_RESOURCES; index += 1) {
        if (state->textures[index].in_use && state->textures[index].resource_id == resource_id) {
            return &state->textures[index];
        }
        if (!empty && !state->textures[index].in_use) {
            empty = &state->textures[index];
        }
    }
    if (!empty) {
        return NULL;
    }
    memset(empty, 0, sizeof(*empty));
    empty->in_use = 1;
    empty->resource_id = resource_id;
    return empty;
}

static GLuint kain_native_ui_win32_gl_ensure_texture(
    KainNativeUiWin32GlState* state,
    KainNativeUiResource* resource
) {
    KainNativeUiWin32GlTextureCacheEntry* entry;
    if (!state || !resource || strcmp(resource->resource_type, "texture") != 0) {
        return 0;
    }
    if (strcmp(resource->aux, "rgba8") != 0) {
        return 0;
    }
    if (!resource->bytes || resource->width <= 0 || resource->height <= 0) {
        return 0;
    }
    entry = kain_native_ui_win32_gl_texture_entry(state, resource->id);
    if (!entry) {
        return 0;
    }
    if (entry->texture_id == 0) {
        glGenTextures(1, &entry->texture_id);
    }
    if (entry->texture_id == 0) {
        return 0;
    }
    if (entry->uploaded_revision != resource->bytes_revision) {
        glBindTexture(GL_TEXTURE_2D, entry->texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            (GLsizei)resource->width,
            (GLsizei)resource->height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            resource->bytes
        );
        entry->uploaded_revision = resource->bytes_revision;
    }
    return entry->texture_id;
}

static void kain_native_ui_win32_gl_draw_textured_quad(
    GLuint texture_id,
    float x,
    float y,
    float width,
    float height,
    float r,
    float g,
    float b,
    float a
) {
    if (texture_id == 0) {
        return;
    }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(x, y);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x + width, y);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x + width, y + height);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(x, y + height);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

static int kain_native_ui_win32_gl_write_bmp(
    const char* path,
    int width,
    int height,
    const uint8_t* rgba
) {
    FILE* file = NULL;
    size_t pixel_bytes;
    size_t image_bytes;
    size_t file_bytes;
    size_t index;
    uint8_t* bgra;
    unsigned char header[54];
    const int32_t bmp_height = -height;

    if (!path || !path[0] || width <= 0 || height <= 0 || !rgba) {
        return 0;
    }
    pixel_bytes = (size_t)width * (size_t)height;
    image_bytes = pixel_bytes * 4u;
    file_bytes = image_bytes + sizeof(header);
    bgra = (uint8_t*)malloc(image_bytes);
    if (!bgra) {
        return 0;
    }
    for (index = 0; index < pixel_bytes; index += 1u) {
        const size_t source = index * 4u;
        bgra[source + 0u] = rgba[source + 2u];
        bgra[source + 1u] = rgba[source + 1u];
        bgra[source + 2u] = rgba[source + 0u];
        bgra[source + 3u] = rgba[source + 3u];
    }

    memset(header, 0, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';
    header[2] = (unsigned char)(file_bytes & 0xffu);
    header[3] = (unsigned char)((file_bytes >> 8u) & 0xffu);
    header[4] = (unsigned char)((file_bytes >> 16u) & 0xffu);
    header[5] = (unsigned char)((file_bytes >> 24u) & 0xffu);
    header[10] = 54;
    header[14] = 40;
    header[18] = (unsigned char)(width & 0xff);
    header[19] = (unsigned char)((width >> 8) & 0xff);
    header[20] = (unsigned char)((width >> 16) & 0xff);
    header[21] = (unsigned char)((width >> 24) & 0xff);
    header[22] = (unsigned char)(bmp_height & 0xff);
    header[23] = (unsigned char)((bmp_height >> 8) & 0xff);
    header[24] = (unsigned char)((bmp_height >> 16) & 0xff);
    header[25] = (unsigned char)((bmp_height >> 24) & 0xff);
    header[26] = 1;
    header[28] = 32;
    header[34] = (unsigned char)(image_bytes & 0xffu);
    header[35] = (unsigned char)((image_bytes >> 8u) & 0xffu);
    header[36] = (unsigned char)((image_bytes >> 16u) & 0xffu);
    header[37] = (unsigned char)((image_bytes >> 24u) & 0xffu);

    #ifdef _WIN32
    if (fopen_s(&file, path, "wb") != 0 || !file) {
    #else
    file = fopen(path, "wb");
    if (!file) {
    #endif
        free(bgra);
        return 0;
    }
    if (fwrite(header, 1, sizeof(header), file) != sizeof(header) ||
        fwrite(bgra, 1, image_bytes, file) != image_bytes) {
        fclose(file);
        free(bgra);
        return 0;
    }
    fclose(file);
    free(bgra);
    return 1;
}

static void kain_native_ui_win32_gl_capture_snapshot_if_requested(
    KainNativeUiWin32GlState* state
) {
    int width;
    int height;
    uint8_t* rgba;
    if (!state || state->screenshot_written || !state->screenshot_path[0]) {
        return;
    }
    if (state->auto_exit_after_frames > 0 && state->frame_counter < state->auto_exit_after_frames) {
        return;
    }
    width = state->session->width > 0 ? (int)state->session->width : 1;
    height = state->session->height > 0 ? (int)state->session->height : 1;
    rgba = (uint8_t*)malloc((size_t)width * (size_t)height * 4u);
    if (!rgba) {
        return;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    if (kain_native_ui_win32_gl_write_bmp(state->screenshot_path, width, height, rgba)) {
        state->screenshot_written = 1;
        state->session->host_should_close = 1;
    }
    free(rgba);
}

static void kain_native_ui_win32_gl_destroy_textures(KainNativeUiWin32GlState* state) {
    int index;
    if (!state) {
        return;
    }
    for (index = 0; index < KAIN_NATIVE_UI_MAX_RESOURCES; index += 1) {
        if (state->textures[index].in_use && state->textures[index].texture_id != 0) {
            glDeleteTextures(1, &state->textures[index].texture_id);
        }
    }
    memset(state->textures, 0, sizeof(state->textures));
}

static void kain_native_ui_win32_gl_render_command(
    KainNativeUiWin32GlState* state,
    KainNativeUiDrawCommand* command
) {
    float r;
    float g;
    float b;
    float a;
    if (strcmp(command->kind, "rect") == 0) {
        kain_native_ui_win32_gl_style_fill_color(
            state->session,
            command->node_id,
            command->style_key,
            0.18f,
            0.20f,
            0.24f,
            1.0f,
            &r,
            &g,
            &b,
            &a
        );
        kain_gl_draw_rect(
            (float)command->x,
            (float)command->y,
            (float)command->width,
            (float)command->height,
            r,
            g,
            b,
            a
        );
        return;
    }
    if (strcmp(command->kind, "text") == 0) {
        const int font_height = kain_native_ui_win32_gl_font_pixel_height(
            state->session,
            command->font_resource_id
        );
        if (font_height != state->surface.font_pixel_height || !state->surface.font_ready) {
            kain_native_ui_win32_gl_rebuild_font(&state->surface, font_height);
        }
        kain_native_ui_win32_gl_style_fill_color(
            state->session,
            command->node_id,
            command->style_key,
            0.95f,
            0.96f,
            0.98f,
            1.0f,
            &r,
            &g,
            &b,
            &a
        );
        glColor4f(r, g, b, a);
        kain_win32_gl_surface_draw_text(
            &state->surface,
            (float)command->x,
            (float)command->y,
            command->text
        );
        return;
    }
    if (strcmp(command->kind, "resource") == 0) {
        KainNativeUiResource* resource = kain_native_ui_win32_gl_find_resource(
            state->session,
            command->resource_id
        );
        if (!resource) {
            return;
        }
        kain_native_ui_win32_gl_style_fill_color(
            state->session,
            command->node_id,
            command->style_key,
            0.28f,
            0.32f,
            0.38f,
            1.0f,
            &r,
            &g,
            &b,
            &a
        );
        if (strcmp(resource->resource_type, "texture") == 0) {
            GLuint texture_id = kain_native_ui_win32_gl_ensure_texture(state, resource);
            if (texture_id != 0) {
                kain_native_ui_win32_gl_draw_textured_quad(
                    texture_id,
                    (float)command->x,
                    (float)command->y,
                    (float)command->width,
                    (float)command->height,
                    r,
                    g,
                    b,
                    a
                );
                return;
            }
        }
        kain_gl_draw_rect(
            (float)command->x,
            (float)command->y,
            (float)command->width,
            (float)command->height,
            r,
            g,
            b,
            a
        );
    }
}

static void kain_native_ui_win32_gl_render(KainNativeUiWin32GlState* state) {
    int64_t index;
    if (!state || !state->hwnd || !state->surface.dc) {
        return;
    }
    glClearColor(0.06f, 0.07f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    kain_native_ui_win32_gl_begin_overlay((int)state->session->width, (int)state->session->height);
    for (index = 0; index < state->session->draw_command_count; index += 1) {
        kain_native_ui_win32_gl_render_command(state, &state->session->draw_commands[index]);
    }
    kain_native_ui_win32_gl_end_overlay();
    kain_win32_gl_surface_present(&state->surface);
    state->frame_counter += 1;
    kain_native_ui_win32_gl_capture_snapshot_if_requested(state);
}

static void kain_native_ui_win32_gl_process_menu(KainNativeUiWin32GlState* state) {
    KainNativeUiMenu* menu;
    int64_t command_result = 0;
    if (!state || state->session->active_menu_id <= 0) {
        return;
    }
    menu = kain_native_ui_win32_gl_find_menu(state->session, state->session->active_menu_id);
    if (!menu || !menu->open) {
        state->session->active_menu_id = 0;
        return;
    }
    if (state->menu_auto_command > 0) {
        command_result = state->menu_auto_command;
    } else {
        HMENU popup = CreatePopupMenu();
        int64_t item_index;
        POINT point;
        if (!popup) {
            menu->open = 0;
            state->session->active_menu_id = 0;
            return;
        }
        for (item_index = 0; item_index < KAIN_NATIVE_UI_MAX_MENU_ITEMS; item_index += 1) {
            if (state->session->menu_items[item_index].in_use &&
                state->session->menu_items[item_index].menu_id == menu->id) {
                AppendMenuA(
                    popup,
                    MF_STRING,
                    (UINT_PTR)state->session->menu_items[item_index].command_id,
                    state->session->menu_items[item_index].label
                );
            }
        }
        point.x = (LONG)menu->x;
        point.y = (LONG)menu->y;
        ClientToScreen(state->hwnd, &point);
        command_result = (int64_t)TrackPopupMenuEx(
            popup,
            TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
            point.x,
            point.y,
            state->hwnd,
            NULL
        );
        DestroyMenu(popup);
    }
    menu->open = 0;
    state->session->active_menu_id = 0;
    if (command_result > 0) {
        kain_native_ui_win32_gl_push_event(
            state->session,
            "menu.command",
            0,
            menu->x,
            menu->y,
            command_result,
            ""
        );
    }
}

static int64_t kain_native_ui_win32_gl_dialog_result_from_message_box(int result) {
    switch (result) {
        case IDOK:
            return 1;
        case IDCANCEL:
            return 0;
        case IDYES:
            return 1;
        case IDNO:
            return 0;
        default:
            return result;
    }
}

static void kain_native_ui_win32_gl_process_dialog(KainNativeUiWin32GlState* state) {
    KainNativeUiDialog* dialog;
    int64_t response_value;
    const char* response_text;
    if (!state || state->session->active_dialog_id <= 0) {
        return;
    }
    dialog = kain_native_ui_win32_gl_find_dialog(state->session, state->session->active_dialog_id);
    if (!dialog || dialog->response_ready) {
        return;
    }
    if (state->dialog_auto_response != 0) {
        response_value = state->dialog_auto_response;
        response_text = response_value > 0 ? "auto-ok" : "auto-cancel";
    } else {
        UINT flags = MB_ICONINFORMATION | MB_OK;
        int message_result;
        if (strcmp(dialog->kind, "confirm") == 0) {
            flags = MB_ICONQUESTION | MB_OKCANCEL;
        }
        message_result = MessageBoxA(
            state->hwnd,
            dialog->message[0] ? dialog->message : "Kain Native UI",
            dialog->title[0] ? dialog->title : "Kain Native UI",
            flags
        );
        response_value = kain_native_ui_win32_gl_dialog_result_from_message_box(message_result);
        response_text = response_value > 0 ? "ok" : "cancel";
    }
    kain_native_ui_dialog_respond(
        state->session->id,
        dialog->id,
        response_value,
        response_text
    );
    kain_native_ui_win32_gl_push_event(
        state->session,
        "dialog.response",
        0,
        0.0,
        0.0,
        response_value,
        response_text
    );
}

static LRESULT CALLBACK kain_native_ui_win32_gl_window_proc(
    HWND hwnd,
    UINT msg,
    WPARAM w_param,
    LPARAM l_param
) {
    KainNativeUiWin32GlState* state = (KainNativeUiWin32GlState*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    if (msg == WM_NCCREATE) {
        CREATESTRUCTA* create_struct = (CREATESTRUCTA*)l_param;
        state = (KainNativeUiWin32GlState*)create_struct->lpCreateParams;
        if (!state) {
            return FALSE;
        }
        state->hwnd = hwnd;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        return TRUE;
    }

    if (!state || !state->session) {
        return DefWindowProcA(hwnd, msg, w_param, l_param);
    }

    switch (msg) {
        case WM_SIZE: {
            state->session->width = LOWORD(l_param) > 0 ? LOWORD(l_param) : state->session->width;
            state->session->height = HIWORD(l_param) > 0 ? HIWORD(l_param) : state->session->height;
            kain_native_ui_win32_gl_push_event(
                state->session,
                "window.resize",
                0,
                (double)state->session->width,
                (double)state->session->height,
                0,
                ""
            );
            return 0;
        }
        case WM_MOUSEMOVE: {
            int target;
            state->last_pointer_x = kain_native_ui_win32_gl_get_x(l_param);
            state->last_pointer_y = kain_native_ui_win32_gl_get_y(l_param);
            target = (int)kain_native_ui_hit_test(
                state->session->id,
                (double)state->last_pointer_x,
                (double)state->last_pointer_y
            );
            kain_native_ui_win32_gl_push_event(
                state->session,
                "pointer.move",
                target,
                (double)state->last_pointer_x,
                (double)state->last_pointer_y,
                0,
                ""
            );
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int64_t target = kain_native_ui_hit_test(
                state->session->id,
                (double)kain_native_ui_win32_gl_get_x(l_param),
                (double)kain_native_ui_win32_gl_get_y(l_param)
            );
            SetCapture(hwnd);
            if (target > 0) {
                kain_native_ui_focus(state->session->id, target);
            }
            kain_native_ui_win32_gl_push_event(
                state->session,
                "pointer.down",
                target,
                (double)kain_native_ui_win32_gl_get_x(l_param),
                (double)kain_native_ui_win32_gl_get_y(l_param),
                0,
                "primary"
            );
            return 0;
        }
        case WM_LBUTTONUP: {
            int64_t target = kain_native_ui_hit_test(
                state->session->id,
                (double)kain_native_ui_win32_gl_get_x(l_param),
                (double)kain_native_ui_win32_gl_get_y(l_param)
            );
            ReleaseCapture();
            kain_native_ui_win32_gl_push_event(
                state->session,
                "pointer.up",
                target,
                (double)kain_native_ui_win32_gl_get_x(l_param),
                (double)kain_native_ui_win32_gl_get_y(l_param),
                0,
                "primary"
            );
            return 0;
        }
        case WM_MOUSEWHEEL: {
            POINT screen;
            POINT client;
            screen.x = kain_native_ui_win32_gl_get_x(l_param);
            screen.y = kain_native_ui_win32_gl_get_y(l_param);
            client = screen;
            ScreenToClient(hwnd, &client);
            kain_native_ui_win32_gl_push_event(
                state->session,
                "pointer.scroll",
                kain_native_ui_hit_test(state->session->id, (double)client.x, (double)client.y),
                (double)client.x,
                (double)client.y,
                (int64_t)GET_WHEEL_DELTA_WPARAM(w_param),
                ""
            );
            return 0;
        }
        case WM_KEYDOWN:
            kain_native_ui_win32_gl_push_event(
                state->session,
                "key.down",
                state->session->focused_node_id,
                (double)state->last_pointer_x,
                (double)state->last_pointer_y,
                (int64_t)w_param,
                ""
            );
            return 0;
        case WM_KEYUP:
            kain_native_ui_win32_gl_push_event(
                state->session,
                "key.up",
                state->session->focused_node_id,
                (double)state->last_pointer_x,
                (double)state->last_pointer_y,
                (int64_t)w_param,
                ""
            );
            return 0;
        case WM_CHAR: {
            char text[8];
            const unsigned int codepoint = (unsigned int)w_param;
            text[0] = (codepoint <= 0x7fU) ? (char)codepoint : '?';
            text[1] = '\0';
            if (state->session->ime_active_node_id > 0) {
                kain_native_ui_ime_commit_text(state->session->id, text);
            }
            kain_native_ui_win32_gl_push_event(
                state->session,
                "text.input",
                state->session->ime_active_node_id > 0
                    ? state->session->ime_active_node_id
                    : state->session->focused_node_id,
                (double)state->last_pointer_x,
                (double)state->last_pointer_y,
                (int64_t)w_param,
                text
            );
            return 0;
        }
        case WM_SETFOCUS:
            kain_native_ui_win32_gl_push_event(
                state->session,
                "focus.gained",
                state->session->focused_node_id,
                0.0,
                0.0,
                0,
                ""
            );
            return 0;
        case WM_KILLFOCUS:
            kain_native_ui_win32_gl_push_event(
                state->session,
                "focus.lost",
                state->session->focused_node_id,
                0.0,
                0.0,
                0,
                ""
            );
            return 0;
        case WM_CLOSE:
            state->session->host_should_close = 1;
            state->session->open = 0;
            kain_native_ui_win32_gl_push_event(
                state->session,
                "window.close",
                0,
                0.0,
                0.0,
                0,
                ""
            );
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            state->session->host_should_close = 1;
            state->session->open = 0;
            state->hwnd = NULL;
            state->window_created = 0;
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
    }

    return DefWindowProcA(hwnd, msg, w_param, l_param);
}

static int kain_native_ui_win32_gl_ensure_window(KainNativeUiWin32GlState* state) {
    RECT rect;
    WNDCLASSA window_class;
    const char* window_title;
    if (!state || !state->session || state->session->host_should_close || !state->session->open) {
        return 0;
    }
    if (state->window_created && state->hwnd) {
        return 1;
    }
    state->instance = GetModuleHandleA(NULL);
    if (!state->class_registered) {
        memset(&window_class, 0, sizeof(window_class));
        window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        window_class.lpfnWndProc = kain_native_ui_win32_gl_window_proc;
        window_class.hInstance = state->instance;
        window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
        window_class.lpszClassName = state->class_name;
        if (!RegisterClassA(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return 0;
        }
        state->class_registered = 1;
    }
    rect.left = 0;
    rect.top = 0;
    rect.right = (LONG)(state->session->width > 0 ? state->session->width : 960);
    rect.bottom = (LONG)(state->session->height > 0 ? state->session->height : 540);
    AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW | WS_VISIBLE, FALSE, 0);
    window_title = state->session->window_title[0]
        ? state->session->window_title
        : state->session->app_name;
    state->hwnd = CreateWindowExA(
        0,
        state->class_name,
        window_title && window_title[0] ? window_title : "Kain Native UI",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL,
        NULL,
        state->instance,
        state
    );
    if (!state->hwnd) {
        return 0;
    }
    if (!kain_win32_gl_surface_boot(state->hwnd, &state->surface, 16)) {
        DestroyWindow(state->hwnd);
        state->hwnd = NULL;
        return 0;
    }
    ShowWindow(state->hwnd, SW_SHOW);
    UpdateWindow(state->hwnd);
    state->window_created = 1;
    return 1;
}

static void kain_native_ui_win32_gl_pump_messages(KainNativeUiWin32GlState* state) {
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            if (state && state->session) {
                state->session->host_should_close = 1;
                state->session->open = 0;
            }
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

int64_t kain_native_ui_win32_gl_attach(KainNativeUiSession* session, const char* backend_id) {
    KainNativeUiWin32GlState* state;
    if (!session) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (session->host_state) {
        return KAIN_NATIVE_UI_OK;
    }
    state = (KainNativeUiWin32GlState*)calloc(1, sizeof(*state));
    if (!state) {
        return KAIN_NATIVE_UI_CAPACITY_EXCEEDED;
    }
    state->session = session;
    state->auto_exit_after_frames = kain_native_ui_win32_gl_int_env(
        KAIN_NATIVE_UI_WIN32_GL_AUTO_EXIT_ENV,
        3
    );
    state->menu_auto_command = kain_native_ui_win32_gl_int_env(
        KAIN_NATIVE_UI_WIN32_GL_MENU_AUTO_ENV,
        0
    );
    state->dialog_auto_response = kain_native_ui_win32_gl_int_env(
        KAIN_NATIVE_UI_WIN32_GL_DIALOG_AUTO_ENV,
        0
    );
    kain_native_ui_win32_gl_read_screenshot_env(
        state->screenshot_path,
        sizeof(state->screenshot_path)
    );
    snprintf(state->class_name, sizeof(state->class_name), "%s.%lld", KAIN_NATIVE_UI_WIN32_GL_CLASS_NAME, (long long)session->id);
    session->host_state = state;
    session->host_attached = 1;
    kain_native_ui_win32_gl_copy_text(session->host_backend, sizeof(session->host_backend), "win32-gl");
    if (backend_id && strcmp(backend_id, "auto") == 0) {
        kain_native_ui_win32_gl_copy_text(session->host_backend, sizeof(session->host_backend), "win32-gl");
    }
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_win32_gl_pump(KainNativeUiSession* session) {
    KainNativeUiWin32GlState* state = session ? (KainNativeUiWin32GlState*)session->host_state : NULL;
    if (!session || !state) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!kain_native_ui_win32_gl_ensure_window(state)) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    kain_native_ui_win32_gl_pump_messages(state);
    kain_native_ui_win32_gl_process_menu(state);
    kain_native_ui_win32_gl_process_dialog(state);
    return KAIN_NATIVE_UI_OK;
}

int64_t kain_native_ui_win32_gl_present(KainNativeUiSession* session) {
    KainNativeUiWin32GlState* state = session ? (KainNativeUiWin32GlState*)session->host_state : NULL;
    if (!session || !state) {
        return KAIN_NATIVE_UI_INVALID_SESSION;
    }
    if (!kain_native_ui_win32_gl_ensure_window(state)) {
        return KAIN_NATIVE_UI_INVALID_ARGUMENT;
    }
    kain_native_ui_win32_gl_pump_messages(state);
    kain_native_ui_win32_gl_process_menu(state);
    kain_native_ui_win32_gl_process_dialog(state);
    kain_native_ui_win32_gl_render(state);
    return KAIN_NATIVE_UI_OK;
}

void kain_native_ui_win32_gl_shutdown(KainNativeUiSession* session) {
    KainNativeUiWin32GlState* state = session ? (KainNativeUiWin32GlState*)session->host_state : NULL;
    if (!state) {
        return;
    }
    session->open = 0;
    session->host_should_close = 1;
    if (state->surface.dc || state->surface.glrc) {
        kain_native_ui_win32_gl_destroy_textures(state);
        kain_win32_gl_surface_shutdown(state->hwnd, &state->surface);
    }
    if (state->hwnd && IsWindow(state->hwnd)) {
        DestroyWindow(state->hwnd);
    }
    if (state->class_registered && state->instance) {
        UnregisterClassA(state->class_name, state->instance);
    }
    free(state);
    session->host_state = NULL;
}

int kain_native_ui_win32_gl_clipboard_set_text(KainNativeUiSession* session, const char* text) {
    HGLOBAL memory;
    size_t text_length;
    void* locked;
    KainNativeUiWin32GlState* state = session ? (KainNativeUiWin32GlState*)session->host_state : NULL;
    if (!state) {
        return 0;
    }
    if (!OpenClipboard(state->hwnd)) {
        return 0;
    }
    EmptyClipboard();
    text_length = strlen(text ? text : "") + 1u;
    memory = GlobalAlloc(GMEM_MOVEABLE, text_length);
    if (!memory) {
        CloseClipboard();
        return 0;
    }
    locked = GlobalLock(memory);
    if (!locked) {
        GlobalFree(memory);
        CloseClipboard();
        return 0;
    }
    memcpy(locked, text ? text : "", text_length);
    GlobalUnlock(memory);
    if (!SetClipboardData(CF_TEXT, memory)) {
        GlobalFree(memory);
        CloseClipboard();
        return 0;
    }
    CloseClipboard();
    return 1;
}

int kain_native_ui_win32_gl_clipboard_get_text(
    KainNativeUiSession* session,
    char* out_text,
    size_t out_text_cap
) {
    HANDLE data;
    const char* text;
    KainNativeUiWin32GlState* state = session ? (KainNativeUiWin32GlState*)session->host_state : NULL;
    if (!state || !out_text || out_text_cap == 0) {
        return 0;
    }
    if (!OpenClipboard(state->hwnd)) {
        return 0;
    }
    data = GetClipboardData(CF_TEXT);
    if (!data) {
        CloseClipboard();
        return 0;
    }
    text = (const char*)GlobalLock(data);
    if (!text) {
        CloseClipboard();
        return 0;
    }
    kain_native_ui_win32_gl_copy_text(out_text, out_text_cap, text);
    GlobalUnlock(data);
    CloseClipboard();
    return 1;
}

#endif
