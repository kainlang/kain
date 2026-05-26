#include "pong_window_bridge.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <GL/gl.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PONG_WINDOW_CLASS_NAME "KainPongStateLatticeWindow"
#define PONG_WINDOW_SCREENSHOT_ENV "PONG_WINDOW_SCREENSHOT_PATH"
#define PONG_WINDOW_SCREENSHOT_FRAME_ENV "PONG_WINDOW_SCREENSHOT_FRAME"

typedef struct {
    HINSTANCE instance;
    HWND hwnd;
    HDC dc;
    HGLRC glrc;
    int width;
    int height;
    int board_width;
    int board_height;
    int frame_budget;
    int should_close;
    int screenshot_written;
    int screenshot_target_frame;
    int frames_presented;
    char screenshot_path[MAX_PATH];
    char last_error[256];
} PongWindowState;

static PongWindowState g_pong_window = {0};

static void pong_copy_text(char* out_text, size_t out_text_cap, const char* text) {
    if (!out_text || out_text_cap == 0) {
        return;
    }
    if (!text) {
        text = "";
    }
    snprintf(out_text, out_text_cap, "%s", text);
}

static void pong_set_error(const char* text) {
    pong_copy_text(g_pong_window.last_error, sizeof(g_pong_window.last_error), text);
}

static float pong_color_component(int value) {
    if (value < 0) {
        value = 0;
    }
    if (value > 255) {
        value = 255;
    }
    return (float)value / 255.0f;
}

static int pong_clamp_int(int value, int low, int high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static int pong_abs_int(int value) {
    return value < 0 ? 0 - value : value;
}

static int pong_swarm_columns(int sample_count) {
    if (sample_count >= 256) {
        return 16;
    }
    if (sample_count >= 160) {
        return 14;
    }
    if (sample_count >= 96) {
        return 12;
    }
    return 8;
}

static int pong_read_env_int(const char* name, int fallback) {
    char buffer[64];
    DWORD length;
    int value;
    if (!name || !name[0]) {
        return fallback;
    }
    length = GetEnvironmentVariableA(name, buffer, (DWORD)sizeof(buffer));
    if (length == 0 || length >= (DWORD)sizeof(buffer)) {
        return fallback;
    }
    value = atoi(buffer);
    if (value <= 0) {
        return fallback;
    }
    return value;
}

static void pong_read_screenshot_config(PongWindowState* state) {
    DWORD length;
    int default_frame;
    if (!state) {
        return;
    }
    state->screenshot_path[0] = '\0';
    length = GetEnvironmentVariableA(
        PONG_WINDOW_SCREENSHOT_ENV,
        state->screenshot_path,
        (DWORD)sizeof(state->screenshot_path)
    );
    if (length == 0 || length >= (DWORD)sizeof(state->screenshot_path)) {
        state->screenshot_path[0] = '\0';
    }
    default_frame = state->frame_budget > 32 ? state->frame_budget - 24 : state->frame_budget;
    if (default_frame <= 0) {
        default_frame = 1;
    }
    state->screenshot_target_frame = pong_read_env_int(PONG_WINDOW_SCREENSHOT_FRAME_ENV, default_frame);
    if (state->screenshot_target_frame > state->frame_budget && state->frame_budget > 0) {
        state->screenshot_target_frame = state->frame_budget;
    }
}

static int pong_write_bmp(const char* path, int width, int height, const uint8_t* rgba) {
    BITMAPFILEHEADER file_header;
    BITMAPINFOHEADER info_header;
    FILE* file;
    int stride;
    int image_bytes;
    int y;

    if (!path || !path[0] || !rgba || width <= 0 || height <= 0) {
        return 0;
    }

    stride = ((width * 3) + 3) & ~3;
    image_bytes = stride * height;
    ZeroMemory(&file_header, sizeof(file_header));
    ZeroMemory(&info_header, sizeof(info_header));

    file_header.bfType = 0x4D42;
    file_header.bfOffBits = sizeof(file_header) + sizeof(info_header);
    file_header.bfSize = file_header.bfOffBits + image_bytes;

    info_header.biSize = sizeof(info_header);
    info_header.biWidth = width;
    info_header.biHeight = height;
    info_header.biPlanes = 1;
    info_header.biBitCount = 24;
    info_header.biCompression = BI_RGB;
    info_header.biSizeImage = image_bytes;

    file = fopen(path, "wb");
    if (!file) {
        pong_set_error("failed to open screenshot path");
        return 0;
    }

    fwrite(&file_header, sizeof(file_header), 1, file);
    fwrite(&info_header, sizeof(info_header), 1, file);

    for (y = 0; y < height; y += 1) {
        int x;
        const uint8_t* row = rgba + ((size_t)y * (size_t)width * 4u);
        for (x = 0; x < width; x += 1) {
            const uint8_t* pixel = row + (size_t)x * 4u;
            uint8_t bgr[3];
            bgr[0] = pixel[2];
            bgr[1] = pixel[1];
            bgr[2] = pixel[0];
            fwrite(bgr, sizeof(bgr), 1, file);
        }
        for (x = width * 3; x < stride; x += 1) {
            fputc(0, file);
        }
    }

    fclose(file);
    return 1;
}

static void pong_capture_screenshot_if_requested(PongWindowState* state) {
    uint8_t* rgba;
    if (!state || state->screenshot_written || !state->screenshot_path[0]) {
        return;
    }
    if (state->frames_presented + 1 < state->screenshot_target_frame) {
        return;
    }
    rgba = (uint8_t*)malloc((size_t)state->width * (size_t)state->height * 4u);
    if (!rgba) {
        pong_set_error("failed to allocate screenshot buffer");
        return;
    }
    glReadPixels(0, 0, state->width, state->height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    if (pong_write_bmp(state->screenshot_path, state->width, state->height, rgba)) {
        state->screenshot_written = 1;
    }
    free(rgba);
}

static void pong_draw_rect(float x, float y, float width, float height, float red, float green, float blue, float alpha) {
    glColor4f(red, green, blue, alpha);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

static void pong_draw_hollow_rect(float x, float y, float width, float height, float thickness, float red, float green, float blue, float alpha) {
    pong_draw_rect(x, y, width, thickness, red, green, blue, alpha);
    pong_draw_rect(x, y + height - thickness, width, thickness, red, green, blue, alpha);
    pong_draw_rect(x, y, thickness, height, red, green, blue, alpha);
    pong_draw_rect(x + width - thickness, y, thickness, height, red, green, blue, alpha);
}

static void pong_draw_bar(
    float x,
    float y,
    float width,
    float height,
    float fill_ratio,
    float base_red,
    float base_green,
    float base_blue,
    float fill_red,
    float fill_green,
    float fill_blue
) {
    float clamped = fill_ratio;
    if (clamped < 0.0f) {
        clamped = 0.0f;
    }
    if (clamped > 1.0f) {
        clamped = 1.0f;
    }
    pong_draw_rect(x, y, width, height, base_red, base_green, base_blue, 0.58f);
    pong_draw_rect(x + 2.0f, y + 2.0f, (width - 4.0f) * clamped, height - 4.0f, fill_red, fill_green, fill_blue, 0.92f);
}

static void pong_draw_score_pips(float start_x, float y, int active_count, float red, float green, float blue) {
    int index;
    for (index = 0; index < 9; index += 1) {
        float x = start_x + (float)index * 18.0f;
        float alpha = index < active_count ? 0.96f : 0.18f;
        pong_draw_rect(x, y, 12.0f, 12.0f, red, green, blue, alpha);
    }
}

static void pong_draw_scanlines(float board_left, float board_top, int board_width, int board_height) {
    int y = 10;
    while (y < board_height - 10) {
        pong_draw_rect(board_left + 4.0f, board_top + (float)y, (float)board_width - 8.0f, 1.0f, 0.08f, 0.32f, 0.22f, 0.34f);
        y += 8;
    }
}

static void pong_draw_center_net(float board_left, float board_top, int board_width, int board_height) {
    int y = 24;
    float center_x = board_left + ((float)board_width * 0.5f) - 2.0f;
    while (y < board_height - 24) {
        pong_draw_rect(center_x, board_top + (float)y, 4.0f, 12.0f, 0.70f, 0.98f, 0.82f, 0.82f);
        y += 22;
    }
}

static void pong_draw_ball_trail(
    float board_left,
    float board_top,
    int board_width,
    int board_height,
    int ball_x,
    int ball_y,
    int ball_dx,
    int ball_dy,
    int ball_size
) {
    int step = 1;
    while (step <= 10) {
        int trail_x = ball_x - (ball_dx * step * 2);
        int trail_y = ball_y - (ball_dy * step * 2);
        if (trail_x >= 0 && trail_x <= board_width - ball_size && trail_y >= 0 && trail_y <= board_height - ball_size) {
            int trail_size = ball_size - step;
            if (trail_size < 3) {
                trail_size = 3;
            }
            pong_draw_rect(
                board_left + (float)trail_x,
                board_top + (float)trail_y,
                (float)trail_size,
                (float)trail_size,
                0.40f,
                0.92f,
                0.78f,
                0.22f
            );
        }
        step += 1;
    }
}

static void pong_draw_swarm(
    float board_left,
    float board_top,
    int board_width,
    int board_height,
    int frame_clock,
    int ball_x,
    int sample_count,
    int chaos_mode,
    int swarm_energy
) {
    int index;
    int clamped_samples = pong_clamp_int(sample_count, 32, 512);
    int column_count = pong_swarm_columns(clamped_samples);
    int row_count = (clamped_samples + column_count - 1) / column_count;
    int usable_width = board_width - 96;
    int usable_height = board_height - 96;
    float step_x;
    float step_y;

    if (usable_width < 16) {
        usable_width = 16;
    }
    if (usable_height < 16) {
        usable_height = 16;
    }
    step_x = (float)usable_width / (float)(column_count > 0 ? column_count : 1);
    step_y = (float)usable_height / (float)(row_count > 0 ? row_count : 1);

    for (index = 0; index < clamped_samples; index += 1) {
        int column = index % column_count;
        int row = index / column_count;
        int orbit = (index * 17 + frame_clock * 5 + ball_x + swarm_energy) % usable_height;
        float x = board_left + 48.0f + ((float)column * step_x);
        float y = board_top + 48.0f + (float)((row * 11 + orbit) % usable_height);
        if (chaos_mode != 0 && (index % 9) == 0) {
            pong_draw_rect(x, y, 3.0f, 3.0f, 1.0f, 0.34f, 0.20f, 0.72f);
        } else {
            pong_draw_rect(x, y, 3.0f, 3.0f, 0.18f, 0.90f, 0.78f, 0.48f);
        }
    }
}

static void pong_draw_topbar_sweep(float width, int frame_clock) {
    int index;
    for (index = 0; index < 28; index += 1) {
        float phase = ((float)frame_clock * 0.09f) + ((float)index * 0.35f);
        float sweep = sinf(phase) * 9.0f;
        float x = 40.0f + ((width - 120.0f) / 28.0f) * (float)index;
        pong_draw_rect(x, 46.0f + sweep, 28.0f, 3.0f, 0.68f, 1.0f, 0.82f, 0.74f);
    }
}

static void pong_render_scene(
    PongWindowState* state,
    int frame_clock,
    int left_paddle_y,
    int right_paddle_y,
    int ball_x,
    int ball_y,
    int ball_dx,
    int ball_dy,
    int left_score,
    int right_score,
    int logical_swarm_count,
    int render_swarm_sample_count,
    int collisions_total,
    int chaos_mode,
    int swarm_energy,
    int entangle_registered,
    int entangle_propagations,
    int paddle_width,
    int paddle_height,
    int ball_size,
    int show_scanlines
) {
    float board_left = ((float)state->width - (float)state->board_width) * 0.5f;
    float board_top = 120.0f;
    float left_panel_width = board_left - 40.0f;
    float right_panel_x = board_left + (float)state->board_width + 18.0f;
    float right_panel_width = (float)state->width - right_panel_x - 22.0f;
    float status_y = (float)state->height - 72.0f;
    float score_left_x = board_left + ((float)state->board_width * 0.18f);
    float score_right_x = board_left + ((float)state->board_width * 0.62f);
    float speed_ratio = (float)(pong_abs_int(ball_dx) + pong_abs_int(ball_dy)) / 24.0f;
    float collision_ratio = (float)collisions_total / 24.0f;
    float sample_ratio = (float)pong_clamp_int(render_swarm_sample_count, 32, 512) / 512.0f;
    float logical_ratio = (float)pong_clamp_int(logical_swarm_count, 0, 160000) / 160000.0f;
    float entangle_ratio = (float)pong_clamp_int(entangle_registered, 0, 18) / 18.0f;
    float entangle_target = frame_clock > 0 ? (float)(frame_clock * 18) : 18.0f;
    float propagation_ratio = entangle_target > 0.0f ? (float)entangle_propagations / entangle_target : 0.0f;
    float chaos_ratio = chaos_mode != 0 ? 1.0f : 0.18f;
    int left_button_armed = (frame_clock % 48) < 12;
    int swarm_button_armed = render_swarm_sample_count >= 256;
    int entangle_button_armed = entangle_registered >= 18;

    if (left_panel_width < 80.0f) {
        left_panel_width = 80.0f;
    }
    if (right_panel_width < 80.0f) {
        right_panel_width = 80.0f;
    }
    if (propagation_ratio > 1.0f) {
        propagation_ratio = 1.0f;
    }

    glViewport(0, 0, state->width, state->height);
    glClearColor(0.015f, 0.02f, 0.025f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (GLdouble)state->width, (GLdouble)state->height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    pong_draw_rect(22.0f, 22.0f, (float)state->width - 44.0f, 52.0f, 0.045f, 0.08f, 0.07f, 0.96f);
    pong_draw_rect(22.0f, 120.0f, left_panel_width, (float)state->height - 208.0f, 0.03f, 0.05f, 0.05f, 0.98f);
    pong_draw_rect(board_left, board_top, (float)state->board_width, (float)state->board_height, 0.01f, 0.02f, 0.02f, 1.0f);
    pong_draw_rect(right_panel_x, 120.0f, right_panel_width, (float)state->height - 208.0f, 0.03f, 0.05f, 0.05f, 0.98f);
    pong_draw_rect(22.0f, status_y, (float)state->width - 44.0f, 34.0f, 0.04f, 0.08f, 0.07f, 0.98f);
    pong_draw_topbar_sweep((float)state->width, frame_clock);

    pong_draw_hollow_rect(board_left, board_top, (float)state->board_width, (float)state->board_height, 2.0f, 0.42f, 0.98f, 0.80f, 0.88f);
    if (chaos_mode != 0) {
        pong_draw_hollow_rect(board_left + 8.0f, board_top + 8.0f, (float)state->board_width - 16.0f, (float)state->board_height - 16.0f, 2.0f, 0.95f, 0.38f, 0.20f, 0.64f);
    }
    if (show_scanlines != 0) {
        pong_draw_scanlines(board_left, board_top, state->board_width, state->board_height);
    }
    pong_draw_center_net(board_left, board_top, state->board_width, state->board_height);
    pong_draw_swarm(
        board_left,
        board_top,
        state->board_width,
        state->board_height,
        frame_clock,
        ball_x,
        render_swarm_sample_count,
        chaos_mode,
        swarm_energy
    );
    pong_draw_ball_trail(
        board_left,
        board_top,
        state->board_width,
        state->board_height,
        ball_x,
        ball_y,
        ball_dx,
        ball_dy,
        ball_size
    );
    pong_draw_rect(board_left + 24.0f, board_top + (float)left_paddle_y, (float)paddle_width, (float)paddle_height, 0.65f, 0.98f, 0.88f, 0.96f);
    pong_draw_rect(board_left + (float)(state->board_width - paddle_width - 24), board_top + (float)right_paddle_y, (float)paddle_width, (float)paddle_height, 1.0f, 0.84f, 0.38f, 0.96f);
    pong_draw_rect(board_left + (float)ball_x, board_top + (float)ball_y, (float)ball_size, (float)ball_size, 0.95f, 1.0f, 0.88f, 1.0f);
    pong_draw_score_pips(score_left_x, 86.0f, pong_clamp_int(left_score, 0, 9), 0.72f, 1.0f, 0.86f);
    pong_draw_score_pips(score_right_x, 86.0f, pong_clamp_int(right_score, 0, 9), 1.0f, 0.82f, 0.42f);

    pong_draw_rect(38.0f, 188.0f, left_panel_width - 34.0f, 42.0f, 0.08f, 0.18f, 0.16f, left_button_armed ? 0.94f : 0.72f);
    pong_draw_rect(38.0f, 246.0f, left_panel_width - 34.0f, 42.0f, chaos_mode != 0 ? 0.16f : 0.08f, chaos_mode != 0 ? 0.42f : 0.18f, chaos_mode != 0 ? 0.34f : 0.16f, chaos_mode != 0 ? 0.94f : 0.72f);
    pong_draw_rect(38.0f, 304.0f, left_panel_width - 34.0f, 42.0f, 0.08f, 0.18f, 0.16f, swarm_button_armed ? 0.94f : 0.72f);
    pong_draw_rect(38.0f, 362.0f, left_panel_width - 34.0f, 42.0f, 0.08f, 0.18f, 0.16f, entangle_button_armed ? 0.94f : 0.72f);
    pong_draw_bar(46.0f, 196.0f, left_panel_width - 50.0f, 10.0f, speed_ratio, 0.06f, 0.10f, 0.10f, 0.84f, 1.0f, 0.88f);
    pong_draw_bar(46.0f, 254.0f, left_panel_width - 50.0f, 10.0f, chaos_ratio, 0.06f, 0.10f, 0.10f, 0.98f, 0.44f, 0.22f);
    pong_draw_bar(46.0f, 312.0f, left_panel_width - 50.0f, 10.0f, sample_ratio, 0.06f, 0.10f, 0.10f, 0.24f, 0.96f, 0.82f);
    pong_draw_bar(46.0f, 370.0f, left_panel_width - 50.0f, 10.0f, entangle_ratio, 0.06f, 0.10f, 0.10f, 0.72f, 1.0f, 0.56f);

    pong_draw_bar(right_panel_x + 18.0f, 188.0f, right_panel_width - 36.0f, 22.0f, (float)(left_score + right_score) / 18.0f, 0.05f, 0.08f, 0.08f, 0.84f, 1.0f, 0.88f);
    pong_draw_bar(right_panel_x + 18.0f, 232.0f, right_panel_width - 36.0f, 22.0f, speed_ratio, 0.05f, 0.08f, 0.08f, 0.78f, 0.94f, 1.0f);
    pong_draw_bar(right_panel_x + 18.0f, 276.0f, right_panel_width - 36.0f, 22.0f, collision_ratio, 0.05f, 0.08f, 0.08f, 1.0f, 0.82f, 0.34f);
    pong_draw_bar(right_panel_x + 18.0f, 320.0f, right_panel_width - 36.0f, 22.0f, sample_ratio, 0.05f, 0.08f, 0.08f, 0.22f, 0.94f, 0.78f);
    pong_draw_bar(right_panel_x + 18.0f, 364.0f, right_panel_width - 36.0f, 22.0f, logical_ratio, 0.05f, 0.08f, 0.08f, 0.16f, 0.72f, 0.58f);
    pong_draw_bar(right_panel_x + 18.0f, 408.0f, right_panel_width - 36.0f, 22.0f, entangle_ratio, 0.05f, 0.08f, 0.08f, 0.74f, 1.0f, 0.58f);
    pong_draw_bar(right_panel_x + 18.0f, 452.0f, right_panel_width - 36.0f, 22.0f, propagation_ratio, 0.05f, 0.08f, 0.08f, 0.98f, 0.98f, 0.68f);
    pong_draw_bar(right_panel_x + 18.0f, 496.0f, right_panel_width - 36.0f, 22.0f, chaos_ratio, 0.05f, 0.08f, 0.08f, 0.98f, 0.42f, 0.24f);

    pong_draw_bar(34.0f, status_y + 10.0f, (float)state->width - 68.0f, 10.0f, propagation_ratio, 0.06f, 0.14f, 0.12f, 0.78f, 1.0f, 0.90f);
    pong_draw_rect(
        34.0f + fmodf((float)frame_clock * 9.0f, (float)state->width - 120.0f),
        status_y + 6.0f,
        18.0f,
        18.0f,
        chaos_mode != 0 ? 1.0f : 0.84f,
        chaos_mode != 0 ? 0.38f : 0.98f,
        chaos_mode != 0 ? 0.22f : 0.76f,
        0.92f
    );
}

static void pong_shutdown_window(PongWindowState* state) {
    if (!state) {
        return;
    }
    if (state->glrc) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(state->glrc);
        state->glrc = NULL;
    }
    if (state->hwnd && state->dc) {
        ReleaseDC(state->hwnd, state->dc);
        state->dc = NULL;
    }
    if (state->hwnd) {
        DestroyWindow(state->hwnd);
        state->hwnd = NULL;
    }
    state->should_close = 1;
}

static LRESULT CALLBACK pong_window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    PongWindowState* state = (PongWindowState*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    switch (message) {
        case WM_NCCREATE: {
            CREATESTRUCTA* create = (CREATESTRUCTA*)l_param;
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
            return DefWindowProcA(hwnd, message, w_param, l_param);
        }
        case WM_CLOSE:
            if (state) {
                state->should_close = 1;
            }
            return 0;
        case WM_DESTROY:
            if (state) {
                state->should_close = 1;
            }
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            if (state) {
                state->width = LOWORD(l_param) > 0 ? (int)LOWORD(l_param) : state->width;
                state->height = HIWORD(l_param) > 0 ? (int)HIWORD(l_param) : state->height;
            }
            return 0;
        default:
            return DefWindowProcA(hwnd, message, w_param, l_param);
    }
}

static int pong_register_class(HINSTANCE instance) {
    WNDCLASSA window_class;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = pong_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    window_class.lpszClassName = PONG_WINDOW_CLASS_NAME;
    if (!RegisterClassA(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        pong_set_error("failed to register pong window class");
        return 0;
    }
    return 1;
}

static int pong_boot_context(PongWindowState* state) {
    PIXELFORMATDESCRIPTOR pfd;
    int pixel_format;

    if (!state || !state->hwnd) {
        pong_set_error("missing window for GL boot");
        return 0;
    }

    state->dc = GetDC(state->hwnd);
    if (!state->dc) {
        pong_set_error("failed to acquire window device context");
        return 0;
    }

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    pixel_format = ChoosePixelFormat(state->dc, &pfd);
    if (!pixel_format) {
        pong_set_error("ChoosePixelFormat failed");
        return 0;
    }
    if (!SetPixelFormat(state->dc, pixel_format, &pfd)) {
        pong_set_error("SetPixelFormat failed");
        return 0;
    }
    state->glrc = wglCreateContext(state->dc);
    if (!state->glrc) {
        pong_set_error("wglCreateContext failed");
        return 0;
    }
    if (!wglMakeCurrent(state->dc, state->glrc)) {
        pong_set_error("wglMakeCurrent failed");
        return 0;
    }

    glDisable(GL_DITHER);
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, state->width, state->height);
    return 1;
}

static int pong_create_window(PongWindowState* state, const char* title) {
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    RECT rect;

    if (!state) {
        pong_set_error("missing pong window state");
        return 0;
    }
    if (!pong_register_class(state->instance)) {
        return 0;
    }

    rect.left = 0;
    rect.top = 0;
    rect.right = state->width > 0 ? state->width : 1460;
    rect.bottom = state->height > 0 ? state->height : 900;
    AdjustWindowRect(&rect, style, FALSE);

    state->hwnd = CreateWindowExA(
        0,
        PONG_WINDOW_CLASS_NAME,
        title && title[0] ? title : "Pong // Quantum State Lattice",
        style,
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
        pong_set_error("failed to create pong window");
        return 0;
    }

    ShowWindow(state->hwnd, SW_SHOW);
    UpdateWindow(state->hwnd);
    return pong_boot_context(state);
}

static void pong_pump_messages(PongWindowState* state) {
    MSG message;
    if (!state) {
        return;
    }
    while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            state->should_close = 1;
            break;
        }
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
}

int pong_window_probe(void) {
    return 1;
}

int pong_window_open_state(
    const char* title,
    int width,
    int height,
    int board_width,
    int board_height,
    int frame_budget
) {
    PongWindowState* state = &g_pong_window;
    pong_window_shutdown();
    ZeroMemory(state, sizeof(*state));
    state->instance = GetModuleHandleA(NULL);
    state->width = width > 0 ? width : 1460;
    state->height = height > 0 ? height : 900;
    state->board_width = board_width > 0 ? board_width : 900;
    state->board_height = board_height > 0 ? board_height : 560;
    state->frame_budget = frame_budget > 0 ? frame_budget : 192;
    state->frames_presented = 0;
    pong_copy_text(state->last_error, sizeof(state->last_error), "ok");
    pong_read_screenshot_config(state);
    if (!pong_create_window(state, title)) {
        pong_shutdown_window(state);
        return 0;
    }
    return 1;
}

int pong_window_present_state(
    int frame_clock,
    int left_paddle_y,
    int right_paddle_y,
    int ball_x,
    int ball_y,
    int ball_dx,
    int ball_dy,
    int left_score,
    int right_score,
    int logical_swarm_count,
    int render_swarm_sample_count,
    int collisions_total,
    int chaos_mode,
    int swarm_energy,
    int entangle_registered,
    int entangle_propagations,
    int paddle_width,
    int paddle_height,
    int ball_size,
    int show_scanlines
) {
    PongWindowState* state = &g_pong_window;
    if (!state->hwnd || !state->dc || !state->glrc) {
        pong_set_error("present called before window open");
        return 0;
    }

    pong_pump_messages(state);
    if (state->should_close) {
        return 1;
    }

    pong_render_scene(
        state,
        frame_clock,
        left_paddle_y,
        right_paddle_y,
        ball_x,
        ball_y,
        ball_dx,
        ball_dy,
        left_score,
        right_score,
        logical_swarm_count,
        render_swarm_sample_count,
        collisions_total,
        chaos_mode,
        swarm_energy,
        entangle_registered,
        entangle_propagations,
        paddle_width,
        paddle_height,
        ball_size,
        show_scanlines
    );
    glFinish();
    pong_capture_screenshot_if_requested(state);
    SwapBuffers(state->dc);
    state->frames_presented += 1;
    return 1;
}

int pong_window_should_close(void) {
    return g_pong_window.should_close;
}

int pong_window_shutdown(void) {
    pong_shutdown_window(&g_pong_window);
    return 1;
}

int pong_window_frames_presented(void) {
    return g_pong_window.frames_presented;
}

int pong_window_write_report(const char* path) {
    FILE* file;
    if (!path || !path[0]) {
        pong_set_error("missing pong window report path");
        return 0;
    }
    file = fopen(path, "wb");
    if (!file) {
        pong_set_error("failed to open pong window report path");
        return 0;
    }
    fprintf(file, "frames=%d\n", g_pong_window.frames_presented);
    fprintf(file, "screenshot_written=%d\n", g_pong_window.screenshot_written);
    fprintf(file, "screenshot_target_frame=%d\n", g_pong_window.screenshot_target_frame);
    fprintf(file, "last_error=%s\n", g_pong_window.last_error[0] ? g_pong_window.last_error : "ok");
    fclose(file);
    return 1;
}

#else

int pong_window_probe(void) {
    return 0;
}

int pong_window_open_state(
    const char* title,
    int width,
    int height,
    int board_width,
    int board_height,
    int frame_budget
) {
    (void)title;
    (void)width;
    (void)height;
    (void)board_width;
    (void)board_height;
    (void)frame_budget;
    return 0;
}

int pong_window_present_state(
    int frame_clock,
    int left_paddle_y,
    int right_paddle_y,
    int ball_x,
    int ball_y,
    int ball_dx,
    int ball_dy,
    int left_score,
    int right_score,
    int logical_swarm_count,
    int render_swarm_sample_count,
    int collisions_total,
    int chaos_mode,
    int swarm_energy,
    int entangle_registered,
    int entangle_propagations,
    int paddle_width,
    int paddle_height,
    int ball_size,
    int show_scanlines
) {
    (void)frame_clock;
    (void)left_paddle_y;
    (void)right_paddle_y;
    (void)ball_x;
    (void)ball_y;
    (void)ball_dx;
    (void)ball_dy;
    (void)left_score;
    (void)right_score;
    (void)logical_swarm_count;
    (void)render_swarm_sample_count;
    (void)collisions_total;
    (void)chaos_mode;
    (void)swarm_energy;
    (void)entangle_registered;
    (void)entangle_propagations;
    (void)paddle_width;
    (void)paddle_height;
    (void)ball_size;
    (void)show_scanlines;
    return 0;
}

int pong_window_should_close(void) {
    return 1;
}

int pong_window_shutdown(void) {
    return 1;
}

int pong_window_frames_presented(void) {
    return 0;
}

int pong_window_write_report(const char* path) {
    (void)path;
    return 0;
}

#endif
