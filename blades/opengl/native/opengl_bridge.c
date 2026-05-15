#include "opengl_bridge.h"

#ifdef _WIN32

#include <windows.h>
#include <GL/gl.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPENGL_BLADE_CLASS_NAME "KainBladeOpenGlWindow"
#define OPENGL_BLADE_SCREENSHOT_ENV "OPENGL_BLADE_SCREENSHOT_PATH"

typedef struct {
    HINSTANCE instance;
    HWND hwnd;
    HDC dc;
    HGLRC glrc;
    int width;
    int height;
    int frame_budget;
    int should_close;
    int screenshot_written;
    float clear_r;
    float clear_g;
    float clear_b;
    float accent_r;
    float accent_g;
    float accent_b;
    char screenshot_path[MAX_PATH];
} OpenGlBladeWindowState;

static OpenGlBladeWindowState g_window_state = {0};
static int g_frames_presented = 0;
static int g_triangles_drawn = 0;
static char g_last_error[256] = "ok";

static void opengl_copy_text(char* out_text, size_t out_text_cap, const char* text) {
    if (!out_text || out_text_cap == 0) {
        return;
    }
    if (!text) {
        text = "";
    }
    snprintf(out_text, out_text_cap, "%s", text);
}

static void opengl_set_error(const char* text) {
    opengl_copy_text(g_last_error, sizeof(g_last_error), text);
}

static float opengl_color_component(int value) {
    if (value < 0) {
        value = 0;
    }
    if (value > 255) {
        value = 255;
    }
    return (float)value / 255.0f;
}

static void opengl_reset_counters(void) {
    g_frames_presented = 0;
    g_triangles_drawn = 0;
    opengl_set_error("ok");
}

static void opengl_read_screenshot_env(OpenGlBladeWindowState* state) {
    DWORD length;
    if (!state) {
        return;
    }
    state->screenshot_path[0] = '\0';
    length = GetEnvironmentVariableA(
        OPENGL_BLADE_SCREENSHOT_ENV,
        state->screenshot_path,
        (DWORD)sizeof(state->screenshot_path)
    );
    if (length == 0 || length >= (DWORD)sizeof(state->screenshot_path)) {
        state->screenshot_path[0] = '\0';
    }
}

static int opengl_write_bmp(const char* path, int width, int height, const uint8_t* rgba) {
    BITMAPFILEHEADER file_header;
    BITMAPINFOHEADER info_header;
    FILE* file;
    int stride;
    int pixel_bytes;
    int image_bytes;
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
        opengl_set_error("failed to open screenshot path");
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

static void opengl_capture_screenshot_if_requested(OpenGlBladeWindowState* state) {
    uint8_t* rgba;
    if (!state || state->screenshot_written || !state->screenshot_path[0] || state->width <= 0 || state->height <= 0) {
        return;
    }
    rgba = (uint8_t*)malloc((size_t)state->width * (size_t)state->height * 4u);
    if (!rgba) {
        opengl_set_error("failed to allocate screenshot buffer");
        return;
    }
    glReadPixels(0, 0, state->width, state->height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    if (opengl_write_bmp(state->screenshot_path, state->width, state->height, rgba)) {
        state->screenshot_written = 1;
    }
    free(rgba);
}

static void opengl_shutdown_window(OpenGlBladeWindowState* state) {
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
}

static LRESULT CALLBACK opengl_window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    OpenGlBladeWindowState* state = (OpenGlBladeWindowState*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
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

static int opengl_register_class(HINSTANCE instance) {
    WNDCLASSA window_class;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = opengl_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorA(NULL, IDC_ARROW);
    window_class.lpszClassName = OPENGL_BLADE_CLASS_NAME;
    if (!RegisterClassA(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        opengl_set_error("failed to register OpenGL blade window class");
        return 0;
    }
    return 1;
}

static int opengl_boot_context(OpenGlBladeWindowState* state) {
    PIXELFORMATDESCRIPTOR pfd;
    int pixel_format;

    if (!state || !state->hwnd) {
        opengl_set_error("missing window for GL boot");
        return 0;
    }

    state->dc = GetDC(state->hwnd);
    if (!state->dc) {
        opengl_set_error("failed to acquire window device context");
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
        opengl_set_error("ChoosePixelFormat failed");
        return 0;
    }
    if (!SetPixelFormat(state->dc, pixel_format, &pfd)) {
        opengl_set_error("SetPixelFormat failed");
        return 0;
    }
    state->glrc = wglCreateContext(state->dc);
    if (!state->glrc) {
        opengl_set_error("wglCreateContext failed");
        return 0;
    }
    if (!wglMakeCurrent(state->dc, state->glrc)) {
        opengl_set_error("wglMakeCurrent failed");
        return 0;
    }

    glDisable(GL_DITHER);
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, state->width, state->height);
    return 1;
}

static int opengl_create_window(OpenGlBladeWindowState* state, const char* title) {
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    RECT rect;

    if (!state) {
        opengl_set_error("missing window state");
        return 0;
    }
    if (!opengl_register_class(state->instance)) {
        return 0;
    }

    rect.left = 0;
    rect.top = 0;
    rect.right = state->width > 0 ? state->width : 1280;
    rect.bottom = state->height > 0 ? state->height : 720;
    AdjustWindowRect(&rect, style, FALSE);

    state->hwnd = CreateWindowExA(
        0,
        OPENGL_BLADE_CLASS_NAME,
        title && title[0] ? title : "OpenGL Blade",
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
        opengl_set_error("failed to create OpenGL blade window");
        return 0;
    }

    ShowWindow(state->hwnd, SW_SHOW);
    UpdateWindow(state->hwnd);
    return opengl_boot_context(state);
}

static void opengl_render_frame(OpenGlBladeWindowState* state, int frame_index) {
    float phase = state->frame_budget > 0 ? (float)frame_index / (float)state->frame_budget : 0.0f;
    float angle = phase * 6.2831853f;
    float orbit_x = cosf(angle) * 0.28f;
    float orbit_y = sinf(angle) * 0.28f;

    glViewport(0, 0, state->width, state->height);
    glClearColor(state->clear_r, state->clear_g, state->clear_b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBegin(GL_TRIANGLES);
    glColor3f(state->accent_r, state->accent_g, state->accent_b);
    glVertex2f(orbit_x, 0.58f + orbit_y * 0.18f);
    glColor3f(state->accent_r * 0.25f, state->accent_g * 0.35f, state->accent_b * 0.65f);
    glVertex2f(-0.64f, -0.48f);
    glColor3f(state->accent_r * 0.85f, state->accent_g * 0.92f, state->accent_b * 0.96f);
    glVertex2f(0.64f, -0.48f);
    glEnd();

    glFinish();
    opengl_capture_screenshot_if_requested(state);
    SwapBuffers(state->dc);
    g_frames_presented += 1;
    g_triangles_drawn += 1;
}

int opengl_native_probe(void) {
    return 1;
}

int opengl_native_run_window(
    const char* title,
    int width,
    int height,
    int frame_budget,
    int clear_red,
    int clear_green,
    int clear_blue,
    int accent_red,
    int accent_green,
    int accent_blue
) {
    MSG message;
    int frame_index = 0;
    OpenGlBladeWindowState* state = &g_window_state;

    ZeroMemory(state, sizeof(*state));
    state->instance = GetModuleHandleA(NULL);
    state->width = width > 0 ? width : 1280;
    state->height = height > 0 ? height : 720;
    state->frame_budget = frame_budget > 0 ? frame_budget : 180;
    state->clear_r = opengl_color_component(clear_red);
    state->clear_g = opengl_color_component(clear_green);
    state->clear_b = opengl_color_component(clear_blue);
    state->accent_r = opengl_color_component(accent_red);
    state->accent_g = opengl_color_component(accent_green);
    state->accent_b = opengl_color_component(accent_blue);
    opengl_read_screenshot_env(state);
    opengl_reset_counters();

    if (!opengl_create_window(state, title)) {
        opengl_shutdown_window(state);
        return 10;
    }

    while (!state->should_close && frame_index < state->frame_budget) {
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
        opengl_render_frame(state, frame_index);
        frame_index += 1;
    }

    opengl_shutdown_window(state);
    return 0;
}

int opengl_native_frames_presented(void) {
    return g_frames_presented;
}

int opengl_native_triangles_drawn(void) {
    return g_triangles_drawn;
}

int opengl_native_write_report(const char* path) {
    FILE* file;
    if (!path || !path[0]) {
        opengl_set_error("missing report path");
        return 0;
    }
    file = fopen(path, "wb");
    if (!file) {
        opengl_set_error("failed to open report path");
        return 0;
    }
    fprintf(file, "frames=%d\n", g_frames_presented);
    fprintf(file, "triangles=%d\n", g_triangles_drawn);
    fprintf(file, "last_error=%s\n", g_last_error);
    fclose(file);
    return 1;
}

#else

int opengl_native_probe(void) {
    return 0;
}

int opengl_native_run_window(
    const char* title,
    int width,
    int height,
    int frame_budget,
    int clear_red,
    int clear_green,
    int clear_blue,
    int accent_red,
    int accent_green,
    int accent_blue
) {
    (void)title;
    (void)width;
    (void)height;
    (void)frame_budget;
    (void)clear_red;
    (void)clear_green;
    (void)clear_blue;
    (void)accent_red;
    (void)accent_green;
    (void)accent_blue;
    return -1;
}

int opengl_native_frames_presented(void) {
    return 0;
}

int opengl_native_triangles_drawn(void) {
    return 0;
}

int opengl_native_write_report(const char* path) {
    (void)path;
    return 0;
}

#endif
