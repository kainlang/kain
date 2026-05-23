#include "neural_lattice_bridge.h"

#ifdef _WIN32

#include <windows.h>
#include <GL/gl.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NEURAL_LATTICE_CLASS_NAME "KainBladeNeuralLatticeWindow"
#define NEURAL_LATTICE_SCREENSHOT_ENV "NEURAL_LATTICE_SCREENSHOT_PATH"
#define NEURAL_LATTICE_FRAME_BUDGET_ENV "NEURAL_LATTICE_FRAME_BUDGET"

typedef struct {
    HINSTANCE instance;
    HWND hwnd;
    HDC dc;
    HGLRC glrc;
    HFONT font;
    GLuint font_list_base;
    int width;
    int height;
    int frame_budget;
    int should_close;
    int screenshot_written;
    int signal;
    int mirror_signal;
    int epoch;
    int lock_state;
    int hot_synapses;
    int actor_echo;
    int ui_hash;
    int graphics_score;
    char screenshot_path[MAX_PATH];
} NeuralLatticeWindowState;

static NeuralLatticeWindowState g_window_state = {0};
static int g_frames_presented = 0;
static int g_cells_drawn = 0;
static char g_last_error[256] = "ok";

static void neural_lattice_copy_text(char* out_text, size_t out_text_cap, const char* text) {
    if (!out_text || out_text_cap == 0) {
        return;
    }
    if (!text) {
        text = "";
    }
    snprintf(out_text, out_text_cap, "%s", text);
}

static void neural_lattice_set_error(const char* text) {
    neural_lattice_copy_text(g_last_error, sizeof(g_last_error), text);
}

static float neural_lattice_unit(int value, int modulus) {
    int lane = value % modulus;
    if (lane < 0) {
        lane += modulus;
    }
    return (float)lane / (float)(modulus > 1 ? (modulus - 1) : 1);
}

static float neural_lattice_clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float neural_lattice_fractf(float value) {
    return value - floorf(value);
}

static int neural_lattice_abs_int(int value) {
    return value < 0 ? -value : value;
}

static void neural_lattice_draw_quad(float left, float top, float right, float bottom, float red, float green, float blue, float alpha) {
    glColor4f(red, green, blue, alpha);
    glBegin(GL_QUADS);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();
}

static void neural_lattice_draw_outline(float left, float top, float right, float bottom, float thickness, float red, float green, float blue, float alpha) {
    neural_lattice_draw_quad(left, top, right, top + thickness, red, green, blue, alpha);
    neural_lattice_draw_quad(left, bottom - thickness, right, bottom, red, green, blue, alpha);
    neural_lattice_draw_quad(left, top, left + thickness, bottom, red, green, blue, alpha);
    neural_lattice_draw_quad(right - thickness, top, right, bottom, red, green, blue, alpha);
}

static void neural_lattice_draw_text(NeuralLatticeWindowState* state, float x, float y, float red, float green, float blue, const char* text) {
    size_t text_length;
    if (!state || !state->font_list_base || !text) {
        return;
    }
    text_length = strlen(text);
    if (text_length == 0) {
        return;
    }
    glColor3f(red, green, blue);
    glRasterPos2f(x, y);
    glListBase(state->font_list_base - 32u);
    glCallLists((GLsizei)text_length, GL_UNSIGNED_BYTE, text);
}

static void neural_lattice_draw_scanlines(int width, int height) {
    int y;
    glBegin(GL_LINES);
    for (y = 0; y < height; y += 4) {
        float alpha = (y % 8 == 0) ? 0.07f : 0.035f;
        glColor4f(0.05f, 0.09f, 0.11f, alpha);
        glVertex2f(0.0f, (float)y + 0.5f);
        glVertex2f((float)width, (float)y + 0.5f);
    }
    glEnd();
}

static void neural_lattice_reset_counters(void) {
    g_frames_presented = 0;
    g_cells_drawn = 0;
    neural_lattice_set_error("ok");
}

static void neural_lattice_read_screenshot_env(NeuralLatticeWindowState* state) {
    DWORD length;
    if (!state) {
        return;
    }
    state->screenshot_path[0] = '\0';
    length = GetEnvironmentVariableA(
        NEURAL_LATTICE_SCREENSHOT_ENV,
        state->screenshot_path,
        (DWORD)sizeof(state->screenshot_path)
    );
    if (length == 0 || length >= (DWORD)sizeof(state->screenshot_path)) {
        state->screenshot_path[0] = '\0';
    }
}

static int neural_lattice_read_frame_budget_env(int fallback_value) {
    char buffer[64];
    DWORD length;
    char* end_ptr = NULL;
    long parsed = 0;

    ZeroMemory(buffer, sizeof(buffer));
    length = GetEnvironmentVariableA(
        NEURAL_LATTICE_FRAME_BUDGET_ENV,
        buffer,
        (DWORD)sizeof(buffer)
    );
    if (length == 0 || length >= (DWORD)sizeof(buffer)) {
        return fallback_value;
    }

    parsed = strtol(buffer, &end_ptr, 10);
    if (end_ptr == buffer || parsed <= 0) {
        return fallback_value;
    }

    if (parsed > 2147483647L) {
        return 2147483647;
    }
    return (int)parsed;
}

static int neural_lattice_resolve_frame_budget(int requested_budget, const NeuralLatticeWindowState* state) {
    int fallback_budget = 0;

    if (state && state->screenshot_path[0]) {
        fallback_budget = requested_budget > 0 ? requested_budget : 180;
    }

    return neural_lattice_read_frame_budget_env(fallback_budget);
}

static int neural_lattice_write_bmp(const char* path, int width, int height, const uint8_t* rgba) {
    BITMAPFILEHEADER file_header;
    BITMAPINFOHEADER info_header;
    FILE* file;
    int stride;
    int image_bytes;
    int pixel_bytes;
    int y;

    if (!path || !path[0] || !rgba || width <= 0 || height <= 0) {
        return 0;
    }

    stride = ((width * 3) + 3) & ~3;
    image_bytes = stride * height;
    pixel_bytes = width * 4;
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
        neural_lattice_set_error("failed to open screenshot path");
        return 0;
    }

    fwrite(&file_header, sizeof(file_header), 1, file);
    fwrite(&info_header, sizeof(info_header), 1, file);

    for (y = 0; y < height; y += 1) {
        int x;
        int source_y = y;
        for (x = 0; x < width; x += 1) {
            const uint8_t* pixel = rgba + (source_y * pixel_bytes) + (x * 4);
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

static void neural_lattice_capture_screenshot_if_requested(NeuralLatticeWindowState* state, int frame_index) {
    uint8_t* rgba;
    if (!state || state->screenshot_written || !state->screenshot_path[0] || state->width <= 0 || state->height <= 0) {
        return;
    }
    if (frame_index < 10) {
        return;
    }
    rgba = (uint8_t*)malloc((size_t)state->width * (size_t)state->height * 4u);
    if (!rgba) {
        neural_lattice_set_error("failed to allocate screenshot buffer");
        return;
    }
    glReadPixels(0, 0, state->width, state->height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    if (neural_lattice_write_bmp(state->screenshot_path, state->width, state->height, rgba)) {
        state->screenshot_written = 1;
    }
    free(rgba);
}

static int neural_lattice_boot_font(NeuralLatticeWindowState* state) {
    HFONT font;
    GLuint list_base;

    if (!state || !state->dc) {
        return 0;
    }

    font = (HFONT)GetStockObject(OEM_FIXED_FONT);
    if (!font) {
        font = (HFONT)GetStockObject(ANSI_FIXED_FONT);
    }
    if (!font) {
        neural_lattice_set_error("failed to acquire retro font");
        return 0;
    }

    list_base = glGenLists(96);
    if (!list_base) {
        neural_lattice_set_error("glGenLists failed for font");
        return 0;
    }

    SelectObject(state->dc, font);
    if (!wglUseFontBitmapsA(state->dc, 32, 96, list_base)) {
        glDeleteLists(list_base, 96);
        neural_lattice_set_error("wglUseFontBitmaps failed");
        return 0;
    }

    state->font = font;
    state->font_list_base = list_base;
    return 1;
}

static void neural_lattice_shutdown_window(NeuralLatticeWindowState* state) {
    if (!state) {
        return;
    }
    if (state->font_list_base) {
        glDeleteLists(state->font_list_base, 96);
        state->font_list_base = 0;
        state->font = NULL;
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
}

static LRESULT CALLBACK neural_lattice_window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    NeuralLatticeWindowState* state = (NeuralLatticeWindowState*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
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

static int neural_lattice_register_class(HINSTANCE instance) {
    WNDCLASSA window_class;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = neural_lattice_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    window_class.lpszClassName = NEURAL_LATTICE_CLASS_NAME;
    if (!RegisterClassA(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        neural_lattice_set_error("failed to register neural lattice window class");
        return 0;
    }
    return 1;
}

static int neural_lattice_boot_context(NeuralLatticeWindowState* state) {
    PIXELFORMATDESCRIPTOR pfd;
    int pixel_format;

    if (!state || !state->hwnd) {
        neural_lattice_set_error("missing window for GL boot");
        return 0;
    }

    state->dc = GetDC(state->hwnd);
    if (!state->dc) {
        neural_lattice_set_error("failed to acquire window device context");
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
        neural_lattice_set_error("ChoosePixelFormat failed");
        return 0;
    }
    if (!SetPixelFormat(state->dc, pixel_format, &pfd)) {
        neural_lattice_set_error("SetPixelFormat failed");
        return 0;
    }
    state->glrc = wglCreateContext(state->dc);
    if (!state->glrc) {
        neural_lattice_set_error("wglCreateContext failed");
        return 0;
    }
    if (!wglMakeCurrent(state->dc, state->glrc)) {
        neural_lattice_set_error("wglMakeCurrent failed");
        return 0;
    }

    glDisable(GL_DITHER);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, state->width, state->height);
    if (!neural_lattice_boot_font(state)) {
        return 0;
    }
    return 1;
}

static int neural_lattice_create_window(NeuralLatticeWindowState* state, const char* title) {
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    RECT rect;

    if (!state) {
        neural_lattice_set_error("missing window state");
        return 0;
    }
    if (!neural_lattice_register_class(state->instance)) {
        return 0;
    }

    rect.left = 0;
    rect.top = 0;
    rect.right = state->width > 0 ? state->width : 1280;
    rect.bottom = state->height > 0 ? state->height : 720;
    AdjustWindowRect(&rect, style, FALSE);

    state->hwnd = CreateWindowExA(
        0,
        NEURAL_LATTICE_CLASS_NAME,
        title && title[0] ? title : "Neural Lattice",
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
        neural_lattice_set_error("failed to create neural lattice window");
        return 0;
    }

    ShowWindow(state->hwnd, SW_SHOW);
    UpdateWindow(state->hwnd);
    return neural_lattice_boot_context(state);
}

static void neural_lattice_update_title(NeuralLatticeWindowState* state, int frame_index) {
    char title[256];
    int delta;
    if (!state || !state->hwnd) {
        return;
    }
    delta = neural_lattice_abs_int(state->signal - state->mirror_signal);
    snprintf(
        title,
        sizeof(title),
        "Neural Entanglement Scope // authority=%d mirror=%d delta=%d epoch=%d frame=%d",
        state->signal,
        state->mirror_signal,
        delta,
        state->epoch,
        frame_index
    );
    SetWindowTextA(state->hwnd, title);
}

static void neural_lattice_render_frame(NeuralLatticeWindowState* state, int frame_index) {
    const int sample_count = 40;
    const int bus_lane_count = 8;
    const int hot_slot_count = 16;
    const float margin = 58.0f;
    const float center_gap = 140.0f;
    const float panel_top = 62.0f;
    float panel_bottom;
    float panel_width;
    float left_panel_left;
    float left_panel_right;
    float right_panel_left;
    float right_panel_right;
    float trace_left;
    float trace_right;
    float trace_top;
    float trace_bottom;
    float trace_width;
    float trace_height;
    float frame_phase;
    float bus_phase;
    float signal_lane;
    float mirror_lane;
    float lock_lane;
    float hot_ratio;
    float delta_lane;
    float amber_red;
    float amber_green;
    float amber_blue;
    float cyan_red;
    float cyan_green;
    float cyan_blue;
    float magenta_red;
    float magenta_green;
    float magenta_blue;
    float center_x;
    float sync_glow;
    float shared_points[sample_count];
    char label_buffer[128];
    int sync_delta;
    int x;
    int y;

    panel_bottom = (float)state->height - 62.0f;
    panel_width = ((float)state->width - (margin * 2.0f) - center_gap) * 0.5f;
    left_panel_left = margin;
    left_panel_right = left_panel_left + panel_width;
    right_panel_left = left_panel_right + center_gap;
    right_panel_right = right_panel_left + panel_width;
    trace_left = left_panel_left + 26.0f;
    trace_right = left_panel_right - 26.0f;
    trace_top = panel_top + 86.0f;
    trace_bottom = panel_bottom - 132.0f;
    trace_width = trace_right - trace_left;
    trace_height = trace_bottom - trace_top;
    frame_phase = (float)frame_index * 0.035f;
    bus_phase = (float)frame_index * 0.0125f;
    signal_lane = neural_lattice_unit(state->signal + state->ui_hash, 2048);
    mirror_lane = neural_lattice_unit(state->mirror_signal + state->graphics_score, 2048);
    lock_lane = neural_lattice_unit(state->lock_state + state->hot_synapses, 4096);
    hot_ratio = neural_lattice_clampf((float)state->hot_synapses / 128.0f, 0.0f, 1.0f);
    sync_delta = neural_lattice_abs_int(state->signal - state->mirror_signal);
    delta_lane = neural_lattice_clampf((float)sync_delta / 512.0f, 0.0f, 1.0f);
    amber_red = 0.98f;
    amber_green = 0.66f + (0.14f * hot_ratio);
    amber_blue = 0.24f;
    cyan_red = 0.18f + (0.10f * delta_lane);
    cyan_green = 0.88f;
    cyan_blue = 0.82f + (0.10f * lock_lane);
    magenta_red = 0.92f;
    magenta_green = 0.24f + (0.05f * hot_ratio);
    magenta_blue = 0.74f;
    center_x = (float)state->width * 0.5f;
    sync_glow = sync_delta == 0 ? 0.85f : 0.35f;

    glViewport(0, 0, state->width, state->height);
    glClearColor(0.028f, 0.020f, 0.050f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)state->width, (double)state->height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBegin(GL_QUADS);
    glColor4f(0.08f, 0.05f + (signal_lane * 0.04f), 0.11f, 1.0f);
    glVertex2f(0.0f, 0.0f);
    glColor4f(0.04f, 0.08f, 0.14f + (mirror_lane * 0.06f), 1.0f);
    glVertex2f((float)state->width, 0.0f);
    glColor4f(0.05f, 0.12f + (lock_lane * 0.04f), 0.11f, 1.0f);
    glVertex2f((float)state->width, (float)state->height);
    glColor4f(0.09f, 0.05f, 0.07f + (hot_ratio * 0.05f), 1.0f);
    glVertex2f(0.0f, (float)state->height);
    glEnd();

    neural_lattice_draw_quad(center_x - 56.0f, 32.0f, center_x + 56.0f, (float)state->height - 32.0f, 0.11f, 0.08f, 0.18f, 0.32f);
    neural_lattice_draw_scanlines(state->width, state->height);
    neural_lattice_draw_outline(18.0f, 18.0f, (float)state->width - 18.0f, (float)state->height - 18.0f, 2.0f, 0.20f, 0.74f, 0.68f, 0.45f);

    neural_lattice_draw_quad(left_panel_left, panel_top, left_panel_right, panel_bottom, 0.05f, 0.05f, 0.08f, 0.86f);
    neural_lattice_draw_quad(right_panel_left, panel_top, right_panel_right, panel_bottom, 0.04f, 0.06f, 0.08f, 0.86f);
    neural_lattice_draw_quad(left_panel_left, panel_top, left_panel_right, panel_top + 42.0f, 0.16f, 0.10f, 0.05f, 0.72f);
    neural_lattice_draw_quad(right_panel_left, panel_top, right_panel_right, panel_top + 42.0f, 0.05f, 0.12f, 0.13f, 0.72f);
    neural_lattice_draw_outline(left_panel_left, panel_top, left_panel_right, panel_bottom, 3.0f, amber_red, amber_green, amber_blue, 0.92f);
    neural_lattice_draw_outline(right_panel_left, panel_top, right_panel_right, panel_bottom, 3.0f, cyan_red, cyan_green, cyan_blue, 0.92f);

    glBegin(GL_LINES);
    for (x = 0; x <= 10; x += 1) {
        float t = (float)x / 10.0f;
        float grid_x = trace_left + (t * trace_width);
        float grid_x_right = right_panel_left + 26.0f + (t * trace_width);
        glColor4f(amber_red, amber_green, amber_blue, 0.12f);
        glVertex2f(grid_x, trace_top);
        glVertex2f(grid_x, trace_bottom);
        glColor4f(cyan_red, cyan_green, cyan_blue, 0.12f);
        glVertex2f(grid_x_right, trace_top);
        glVertex2f(grid_x_right, trace_bottom);
    }
    for (y = 0; y <= 7; y += 1) {
        float t = (float)y / 7.0f;
        float grid_y = trace_top + (t * trace_height);
        glColor4f(amber_red, amber_green, amber_blue, 0.12f);
        glVertex2f(trace_left, grid_y);
        glVertex2f(trace_right, grid_y);
        glColor4f(cyan_red, cyan_green, cyan_blue, 0.12f);
        glVertex2f(right_panel_left + 26.0f, grid_y);
        glVertex2f(right_panel_right - 26.0f, grid_y);
    }
    glEnd();

    for (x = 0; x < sample_count; x += 1) {
        float t = (float)x / (float)(sample_count - 1);
        float primary_wave;
        float secondary_wave;
        float envelope;
        primary_wave = sinf((t * 10.995574f) + frame_phase + (signal_lane * 6.2831853f));
        secondary_wave = cosf((t * 24.138f) - (frame_phase * 1.35f) + ((float)(state->actor_echo & 1023) * 0.009f));
        envelope = 0.50f + (primary_wave * (0.26f + (hot_ratio * 0.12f))) + (secondary_wave * (0.08f + (lock_lane * 0.04f)));
        shared_points[x] = neural_lattice_clampf(envelope, 0.10f, 0.90f);
    }

    glLineWidth(4.0f);
    glBegin(GL_LINE_STRIP);
    for (x = 0; x < sample_count; x += 1) {
        float t = (float)x / (float)(sample_count - 1);
        float px = trace_left + (t * trace_width);
        float py = trace_top + (shared_points[x] * trace_height);
        glColor4f(amber_red, amber_green, amber_blue, 0.26f);
        glVertex2f(px, py);
    }
    glEnd();
    glBegin(GL_LINE_STRIP);
    for (x = 0; x < sample_count; x += 1) {
        float t = (float)x / (float)(sample_count - 1);
        float px = right_panel_right - 26.0f - (t * trace_width);
        float py = trace_top + (shared_points[x] * trace_height);
        glColor4f(cyan_red, cyan_green, cyan_blue, 0.26f);
        glVertex2f(px, py);
    }
    glEnd();

    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
    for (x = 0; x < sample_count; x += 1) {
        float t = (float)x / (float)(sample_count - 1);
        float px = trace_left + (t * trace_width);
        float py = trace_top + (shared_points[x] * trace_height);
        glColor4f(amber_red, amber_green, amber_blue, 0.96f);
        glVertex2f(px, py);
    }
    glEnd();
    glBegin(GL_LINE_STRIP);
    for (x = 0; x < sample_count; x += 1) {
        float t = (float)x / (float)(sample_count - 1);
        float px = right_panel_right - 26.0f - (t * trace_width);
        float py = trace_top + (shared_points[x] * trace_height);
        glColor4f(cyan_red, cyan_green, cyan_blue, 0.96f);
        glVertex2f(px, py);
    }
    glEnd();

    glLineWidth(1.5f);
    glBegin(GL_LINES);
    for (y = 0; y < bus_lane_count; y += 1) {
        float lane_t = (float)y / (float)(bus_lane_count - 1);
        float lane_y = trace_top + 24.0f + (lane_t * (trace_height - 48.0f));
        glColor4f(amber_red, amber_green, amber_blue, 0.28f + (sync_glow * 0.18f));
        glVertex2f(left_panel_right + 8.0f, lane_y);
        glColor4f(cyan_red, cyan_green, cyan_blue, 0.28f + (sync_glow * 0.18f));
        glVertex2f(right_panel_left - 8.0f, lane_y);
    }
    glEnd();
    for (y = 0; y < bus_lane_count; y += 1) {
        float lane_t = (float)y / (float)(bus_lane_count - 1);
        float lane_y = trace_top + 24.0f + (lane_t * (trace_height - 48.0f));
        float pulse = neural_lattice_fractf(bus_phase + ((float)y * 0.127f));
        float pulse_x = left_panel_right + 16.0f + pulse * ((right_panel_left - 16.0f) - (left_panel_right + 16.0f));
        neural_lattice_draw_quad(pulse_x - 4.0f, lane_y - 3.0f, pulse_x + 4.0f, lane_y + 3.0f, magenta_red, magenta_green, magenta_blue, 0.90f);
    }

    neural_lattice_draw_quad(center_x - 24.0f, ((float)state->height * 0.5f) - 24.0f, center_x + 24.0f, ((float)state->height * 0.5f) + 24.0f, 0.10f, 0.07f, 0.16f, 0.86f);
    neural_lattice_draw_outline(center_x - 24.0f, ((float)state->height * 0.5f) - 24.0f, center_x + 24.0f, ((float)state->height * 0.5f) + 24.0f, 2.0f, sync_delta == 0 ? 0.48f : 0.88f, sync_delta == 0 ? 0.94f : 0.30f, sync_delta == 0 ? 0.66f : 0.72f, 0.94f);

    for (x = 0; x < hot_slot_count; x += 1) {
        float slot_width = (trace_width - 20.0f) / (float)hot_slot_count;
        float slot_left_left = trace_left + 10.0f + ((float)x * slot_width);
        float slot_left_right = slot_left_left + slot_width - 4.0f;
        float slot_right_right = right_panel_right - 36.0f - ((float)x * slot_width);
        float slot_right_left = slot_right_right - slot_width + 4.0f;
        int lit = x < (int)floorf(hot_ratio * (float)hot_slot_count + 0.5f);
        float glow = lit ? 0.85f : 0.14f;
        neural_lattice_draw_quad(slot_left_left, panel_bottom - 78.0f, slot_left_right, panel_bottom - 52.0f, amber_red, amber_green, amber_blue, glow);
        neural_lattice_draw_quad(slot_right_left, panel_bottom - 78.0f, slot_right_right, panel_bottom - 52.0f, cyan_red, cyan_green, cyan_blue, glow);
        neural_lattice_draw_outline(slot_left_left, panel_bottom - 78.0f, slot_left_right, panel_bottom - 52.0f, 1.0f, amber_red, amber_green, amber_blue, 0.30f);
        neural_lattice_draw_outline(slot_right_left, panel_bottom - 78.0f, slot_right_right, panel_bottom - 52.0f, 1.0f, cyan_red, cyan_green, cyan_blue, 0.30f);
    }

    neural_lattice_draw_text(state, left_panel_left + 18.0f, panel_top + 26.0f, amber_red, amber_green, amber_blue, "AUTHORITY");
    neural_lattice_draw_text(state, right_panel_left + 18.0f, panel_top + 26.0f, cyan_red, cyan_green, cyan_blue, "MIRROR");
    neural_lattice_draw_text(state, center_x - 44.0f, panel_top + 24.0f, magenta_red, magenta_green, magenta_blue, "ENTANGLE");

    snprintf(label_buffer, sizeof(label_buffer), "SIGNAL %d", state->signal);
    neural_lattice_draw_text(state, left_panel_left + 18.0f, panel_top + 58.0f, 0.96f, 0.89f, 0.74f, label_buffer);
    snprintf(label_buffer, sizeof(label_buffer), "SIGNAL %d", state->mirror_signal);
    neural_lattice_draw_text(state, right_panel_left + 18.0f, panel_top + 58.0f, 0.78f, 0.97f, 0.94f, label_buffer);
    snprintf(label_buffer, sizeof(label_buffer), "HOT %d", state->hot_synapses);
    neural_lattice_draw_text(state, left_panel_left + 18.0f, panel_bottom - 26.0f, 0.96f, 0.89f, 0.74f, label_buffer);
    snprintf(label_buffer, sizeof(label_buffer), "ECHO %d", state->actor_echo);
    neural_lattice_draw_text(state, right_panel_left + 18.0f, panel_bottom - 26.0f, 0.78f, 0.97f, 0.94f, label_buffer);

    snprintf(label_buffer, sizeof(label_buffer), "DELTA %d", sync_delta);
    neural_lattice_draw_text(state, center_x - 34.0f, ((float)state->height * 0.5f) - 40.0f, 0.98f, 0.92f, 0.78f, label_buffer);
    snprintf(label_buffer, sizeof(label_buffer), "EPOCH %d", state->epoch);
    neural_lattice_draw_text(state, center_x - 34.0f, ((float)state->height * 0.5f) - 14.0f, 0.98f, 0.92f, 0.78f, label_buffer);
    snprintf(label_buffer, sizeof(label_buffer), "LOCK %d", state->lock_state);
    neural_lattice_draw_text(state, center_x - 30.0f, ((float)state->height * 0.5f) + 12.0f, sync_delta == 0 ? 0.60f : 0.96f, sync_delta == 0 ? 0.94f : 0.32f, sync_delta == 0 ? 0.72f : 0.76f, label_buffer);
    neural_lattice_draw_text(state, center_x - 48.0f, panel_bottom - 24.0f, 0.96f, 0.52f, 0.84f, sync_delta == 0 ? "SYNC LOCKED" : "SYNC DRIFT");

    neural_lattice_update_title(state, frame_index);
    glFinish();
    neural_lattice_capture_screenshot_if_requested(state, frame_index);
    SwapBuffers(state->dc);
    g_frames_presented += 1;
    g_cells_drawn += (sample_count * 2) + (bus_lane_count * 2) + (hot_slot_count * 2);
}

int neural_lattice_native_probe(void) {
    return 1;
}

int neural_lattice_native_run_window(
    const char* title,
    int width,
    int height,
    int frame_budget,
    int signal,
    int mirror_signal,
    int epoch,
    int lock_state,
    int hot_synapses,
    int actor_echo,
    int ui_hash,
    int graphics_score
) {
    MSG message;
    int frame_index = 0;
    NeuralLatticeWindowState* state = &g_window_state;

    ZeroMemory(state, sizeof(*state));
    state->instance = GetModuleHandleA(NULL);
    state->width = width > 0 ? width : 1280;
    state->height = height > 0 ? height : 720;
    state->frame_budget = frame_budget > 0 ? frame_budget : 180;
    state->signal = signal;
    state->mirror_signal = mirror_signal;
    state->epoch = epoch;
    state->lock_state = lock_state;
    state->hot_synapses = hot_synapses;
    state->actor_echo = actor_echo;
    state->ui_hash = ui_hash;
    state->graphics_score = graphics_score;
    neural_lattice_read_screenshot_env(state);
    state->frame_budget = neural_lattice_resolve_frame_budget(frame_budget, state);
    neural_lattice_reset_counters();

    if (!neural_lattice_create_window(state, title)) {
        neural_lattice_shutdown_window(state);
        return 10;
    }

    while (!state->should_close && (state->frame_budget <= 0 || frame_index < state->frame_budget)) {
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                state->should_close = 1;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        if (state->should_close) {
            break;
        }
        neural_lattice_render_frame(state, frame_index);
        frame_index += 1;
        Sleep(16);
    }

    neural_lattice_shutdown_window(state);
    return 0;
}

int neural_lattice_native_frames_presented(void) {
    return g_frames_presented;
}

int neural_lattice_native_cells_drawn(void) {
    return g_cells_drawn;
}

int neural_lattice_native_write_report(const char* path) {
    FILE* file;
    if (!path || !path[0]) {
        neural_lattice_set_error("missing report path");
        return 0;
    }
    file = fopen(path, "wb");
    if (!file) {
        neural_lattice_set_error("failed to open report path");
        return 0;
    }
    fprintf(file, "frames=%d\n", g_frames_presented);
    fprintf(file, "cells=%d\n", g_cells_drawn);
    fprintf(file, "last_error=%s\n", g_last_error);
    fclose(file);
    return 1;
}

#else

int neural_lattice_native_probe(void) {
    return 0;
}

int neural_lattice_native_run_window(
    const char* title,
    int width,
    int height,
    int frame_budget,
    int signal,
    int mirror_signal,
    int epoch,
    int lock_state,
    int hot_synapses,
    int actor_echo,
    int ui_hash,
    int graphics_score
) {
    (void)title;
    (void)width;
    (void)height;
    (void)frame_budget;
    (void)signal;
    (void)mirror_signal;
    (void)epoch;
    (void)lock_state;
    (void)hot_synapses;
    (void)actor_echo;
    (void)ui_hash;
    (void)graphics_score;
    return -1;
}

int neural_lattice_native_frames_presented(void) {
    return 0;
}

int neural_lattice_native_cells_drawn(void) {
    return 0;
}

int neural_lattice_native_write_report(const char* path) {
    (void)path;
    return 0;
}

#endif
