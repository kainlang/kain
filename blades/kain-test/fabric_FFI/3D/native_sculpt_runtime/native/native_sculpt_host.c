#include "native_sculpt_host.h"

#ifdef _WIN32
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>

typedef struct NativeSculptRuntime {
    HWND hwnd;
    int width;
    int height;
    int radius;
    int intensity;
    int hardness;
    int target_polys;
    int frame_count;
    int message_count;
    int mouse_move_count;
    int last_brush_x;
    int last_brush_y;
    int average_fps_x100;
    int checksum;
    int running;
    char title[128];
    char signature[160];
    BITMAPINFO bmi;
    uint32_t* pixels;
    LARGE_INTEGER perf_freq;
} NativeSculptRuntime;

static uint32_t nsh_rgba(int r, int g, int b) {
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (uint32_t)(b | (g << 8) | (r << 16) | (0xFFu << 24));
}

static void nsh_set_pixel(NativeSculptRuntime* runtime, int x, int y, uint32_t color) {
    if (!runtime || !runtime->pixels) {
        return;
    }
    if (x < 0 || y < 0 || x >= runtime->width || y >= runtime->height) {
        return;
    }
    runtime->pixels[y * runtime->width + x] = color;
}

static void nsh_fill_rect(NativeSculptRuntime* runtime, int x0, int y0, int x1, int y1, uint32_t color) {
    int x;
    int y;
    if (x0 > x1) {
        int tmp = x0;
        x0 = x1;
        x1 = tmp;
    }
    if (y0 > y1) {
        int tmp = y0;
        y0 = y1;
        y1 = tmp;
    }
    for (y = y0; y <= y1; ++y) {
        for (x = x0; x <= x1; ++x) {
            nsh_set_pixel(runtime, x, y, color);
        }
    }
}

static void nsh_fill_circle(NativeSculptRuntime* runtime, int cx, int cy, int radius, uint32_t color) {
    int x;
    int y;
    int rr = radius * radius;
    for (y = cy - radius; y <= cy + radius; ++y) {
        for (x = cx - radius; x <= cx + radius; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx * dx + dy * dy <= rr) {
                nsh_set_pixel(runtime, x, y, color);
            }
        }
    }
}

static void nsh_stroke_circle(NativeSculptRuntime* runtime, int cx, int cy, int radius, int thickness, uint32_t color) {
    int x;
    int y;
    int outer = radius * radius;
    int inner_radius = radius - thickness;
    int inner = inner_radius > 0 ? inner_radius * inner_radius : 0;
    for (y = cy - radius; y <= cy + radius; ++y) {
        for (x = cx - radius; x <= cx + radius; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            int d2 = dx * dx + dy * dy;
            if (d2 <= outer && d2 >= inner) {
                nsh_set_pixel(runtime, x, y, color);
            }
        }
    }
}

static void nsh_fill_ellipse(NativeSculptRuntime* runtime, int cx, int cy, int rx, int ry, uint32_t color) {
    int x;
    int y;
    double inv_rx = 1.0 / (double)(rx * rx);
    double inv_ry = 1.0 / (double)(ry * ry);
    for (y = cy - ry; y <= cy + ry; ++y) {
        for (x = cx - rx; x <= cx + rx; ++x) {
            double dx = (double)(x - cx);
            double dy = (double)(y - cy);
            double value = dx * dx * inv_rx + dy * dy * inv_ry;
            if (value <= 1.0) {
                nsh_set_pixel(runtime, x, y, color);
            }
        }
    }
}

static void nsh_draw_background(NativeSculptRuntime* runtime) {
    int x;
    int y;
    for (y = 0; y < runtime->height; ++y) {
        double t = (double)y / (double)(runtime->height > 1 ? runtime->height - 1 : 1);
        uint32_t row = nsh_rgba(
            (int)(10.0 + (26.0 * (1.0 - t))),
            (int)(14.0 + (34.0 * (1.0 - t))),
            (int)(22.0 + (64.0 * (1.0 - t)))
        );
        for (x = 0; x < runtime->width; ++x) {
            runtime->pixels[y * runtime->width + x] = row;
        }
    }
}

static void nsh_draw_grid(NativeSculptRuntime* runtime) {
    int x;
    int y;
    uint32_t line = nsh_rgba(28, 42, 62);
    for (x = 40; x < runtime->width; x += 56) {
        for (y = 24; y < runtime->height - 24; ++y) {
            nsh_set_pixel(runtime, x, y, line);
        }
    }
    for (y = 32; y < runtime->height - 24; y += 48) {
        for (x = 24; x < runtime->width - 24; ++x) {
            nsh_set_pixel(runtime, x, y, line);
        }
    }
}

static void nsh_draw_bust(NativeSculptRuntime* runtime) {
    int cx = runtime->width / 2;
    int cy = runtime->height / 2 + 12;
    nsh_fill_ellipse(runtime, cx, cy + 18, 120, 146, nsh_rgba(36, 78, 124));
    nsh_fill_ellipse(runtime, cx, cy - 6, 88, 116, nsh_rgba(56, 108, 164));
    nsh_fill_ellipse(runtime, cx - 40, cy - 38, 12, 8, nsh_rgba(214, 234, 255));
    nsh_fill_ellipse(runtime, cx + 40, cy - 38, 12, 8, nsh_rgba(214, 234, 255));
    nsh_fill_rect(runtime, cx - 8, cy + 18, cx + 8, cy + 72, nsh_rgba(60, 112, 168));
    nsh_fill_rect(runtime, cx - 62, cy + 92, cx + 62, cy + 114, nsh_rgba(34, 70, 112));
    nsh_fill_circle(runtime, cx + 26, cy - 12, 18, nsh_rgba(82, 148, 220));
    nsh_fill_circle(runtime, cx - 18, cy - 28, 12, nsh_rgba(76, 136, 208));
}

static void nsh_draw_tool_rail(NativeSculptRuntime* runtime) {
    int y;
    nsh_fill_rect(runtime, 18, 18, 166, runtime->height - 18, nsh_rgba(16, 20, 30));
    for (y = 54; y < 54 + 6 * 54; y += 54) {
        nsh_fill_rect(runtime, 34, y, 150, y + 34, y == 54 ? nsh_rgba(190, 104, 28) : nsh_rgba(28, 34, 46));
    }
    for (y = 0; y < 5; ++y) {
        nsh_fill_rect(runtime, 32 + y * 22, runtime->height - 104, 44 + y * 22, runtime->height - 36, nsh_rgba(44 + y * 10, 128 + y * 6, 214));
    }
}

static void nsh_draw_brush(NativeSculptRuntime* runtime, double time_seconds) {
    int cx = (int)((double)runtime->width * 0.62 + cos(time_seconds * 2.2) * 78.0);
    int cy = (int)((double)runtime->height * 0.38 + sin(time_seconds * 1.7) * 54.0);
    int glow_radius = 14 + (runtime->intensity / 4);
    int ring_radius = runtime->radius + 18;
    int ring_thickness = 2 + runtime->hardness / 32;
    runtime->last_brush_x = cx;
    runtime->last_brush_y = cy;
    nsh_fill_circle(runtime, cx, cy, glow_radius, nsh_rgba(74, 188, 248));
    nsh_stroke_circle(runtime, cx, cy, ring_radius, ring_thickness, nsh_rgba(249, 115, 22));
}

static void nsh_render_frame(NativeSculptRuntime* runtime, double time_seconds) {
    nsh_draw_background(runtime);
    nsh_draw_grid(runtime);
    nsh_draw_tool_rail(runtime);
    nsh_draw_bust(runtime);
    nsh_draw_brush(runtime, time_seconds);
}

static void nsh_present_frame(NativeSculptRuntime* runtime) {
    HDC dc;
    if (!runtime || !runtime->hwnd) {
        return;
    }
    dc = GetDC(runtime->hwnd);
    if (!dc) {
        return;
    }
    StretchDIBits(
        dc,
        0,
        0,
        runtime->width,
        runtime->height,
        0,
        0,
        runtime->width,
        runtime->height,
        runtime->pixels,
        &runtime->bmi,
        DIB_RGB_COLORS,
        SRCCOPY
    );
    ReleaseDC(runtime->hwnd, dc);
}

static int nsh_write_bmp(const char* path, NativeSculptRuntime* runtime) {
    BITMAPFILEHEADER file_header;
    BITMAPINFOHEADER info_header;
    FILE* file;
    int y;
    int row_size = runtime->width * 4;
    if (!path || !path[0] || !runtime || !runtime->pixels) {
        return 0;
    }
    file = fopen(path, "wb");
    if (!file) {
        return 0;
    }
    ZeroMemory(&file_header, sizeof(file_header));
    ZeroMemory(&info_header, sizeof(info_header));
    file_header.bfType = 0x4D42;
    file_header.bfOffBits = sizeof(file_header) + sizeof(info_header);
    file_header.bfSize = file_header.bfOffBits + row_size * runtime->height;
    info_header.biSize = sizeof(info_header);
    info_header.biWidth = runtime->width;
    info_header.biHeight = runtime->height;
    info_header.biPlanes = 1;
    info_header.biBitCount = 32;
    info_header.biCompression = BI_RGB;
    fwrite(&file_header, sizeof(file_header), 1, file);
    fwrite(&info_header, sizeof(info_header), 1, file);
    for (y = runtime->height - 1; y >= 0; --y) {
        fwrite(runtime->pixels + (y * runtime->width), row_size, 1, file);
    }
    fclose(file);
    return 1;
}

static int nsh_checksum_pixels(NativeSculptRuntime* runtime) {
    int total = runtime->width * runtime->height;
    int index;
    uint32_t hash = 2166136261u;
    const unsigned char* bytes = (const unsigned char*)runtime->pixels;
    for (index = 0; index < total * 4; ++index) {
        hash ^= bytes[index];
        hash *= 16777619u;
    }
    return (int)(hash & 0x7FFFFFFF);
}

static LRESULT CALLBACK nsh_window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    NativeSculptRuntime* runtime = (NativeSculptRuntime*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    (void)w_param;
    if (message == WM_NCCREATE) {
        CREATESTRUCTA* create_struct = (CREATESTRUCTA*)l_param;
        runtime = (NativeSculptRuntime*)create_struct->lpCreateParams;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)runtime);
        if (runtime) {
            runtime->hwnd = hwnd;
        }
        return TRUE;
    }
    if (runtime) {
        runtime->message_count += 1;
    }
    switch (message) {
        case WM_MOUSEMOVE:
            if (runtime) {
                runtime->mouse_move_count += 1;
                runtime->last_brush_x = GET_X_LPARAM(l_param);
                runtime->last_brush_y = GET_Y_LPARAM(l_param);
            }
            return 0;
        case WM_CLOSE:
            if (runtime) {
                runtime->running = 0;
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (runtime) {
                runtime->running = 0;
            }
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProcA(hwnd, message, w_param, l_param);
}

NSH_EXPORT void* native_sculpt_runtime_create(
    int width,
    int height,
    int radius,
    int intensity,
    int hardness,
    int target_polys,
    const char* title
) {
    NativeSculptRuntime* runtime = (NativeSculptRuntime*)calloc(1, sizeof(NativeSculptRuntime));
    if (!runtime) {
        return NULL;
    }
    runtime->width = width > 64 ? width : 960;
    runtime->height = height > 64 ? height : 640;
    runtime->radius = radius > 1 ? radius : 64;
    runtime->intensity = intensity > 1 ? intensity : 72;
    runtime->hardness = hardness > 1 ? hardness : 54;
    runtime->target_polys = target_polys > 0 ? target_polys : 240000;
    runtime->running = 1;
    strncpy(runtime->title, title && title[0] ? title : "Kain Native Sculpt Runtime", sizeof(runtime->title) - 1);
    runtime->pixels = (uint32_t*)calloc((size_t)runtime->width * (size_t)runtime->height, sizeof(uint32_t));
    if (!runtime->pixels) {
        free(runtime);
        return NULL;
    }
    ZeroMemory(&runtime->bmi, sizeof(runtime->bmi));
    runtime->bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    runtime->bmi.bmiHeader.biWidth = runtime->width;
    runtime->bmi.bmiHeader.biHeight = -runtime->height;
    runtime->bmi.bmiHeader.biPlanes = 1;
    runtime->bmi.bmiHeader.biBitCount = 32;
    runtime->bmi.bmiHeader.biCompression = BI_RGB;
    QueryPerformanceFrequency(&runtime->perf_freq);
    snprintf(
        runtime->signature,
        sizeof(runtime->signature),
        "native-sculpt:%dx%d:r%d:i%d:h%d:p%d",
        runtime->width,
        runtime->height,
        runtime->radius,
        runtime->intensity,
        runtime->hardness,
        runtime->target_polys
    );
    return runtime;
}

NSH_EXPORT int native_sculpt_runtime_run(void* runtime_handle, int duration_ms, const char* capture_bmp_path) {
    NativeSculptRuntime* runtime = (NativeSculptRuntime*)runtime_handle;
    HINSTANCE instance;
    WNDCLASSA wc;
    MSG msg;
    LARGE_INTEGER start_counter;
    LARGE_INTEGER current_counter;
    double elapsed_ms = 0.0;
    int window_width;
    int window_height;
    if (!runtime || !runtime->pixels) {
        return 0;
    }
    instance = GetModuleHandleA(NULL);
    ZeroMemory(&wc, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = nsh_window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "KainNativeSculptRuntimeWindow";
    RegisterClassA(&wc);
    window_width = runtime->width + 16;
    window_height = runtime->height + 39;
    runtime->hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        runtime->title,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        window_width,
        window_height,
        NULL,
        NULL,
        instance,
        runtime
    );
    if (!runtime->hwnd) {
        UnregisterClassA(wc.lpszClassName, instance);
        return 0;
    }
    ShowWindow(runtime->hwnd, SW_SHOW);
    UpdateWindow(runtime->hwnd);
    QueryPerformanceCounter(&start_counter);
    while (runtime->running) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                runtime->running = 0;
            } else {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }
        QueryPerformanceCounter(&current_counter);
        elapsed_ms = ((double)(current_counter.QuadPart - start_counter.QuadPart) * 1000.0) /
            (double)(runtime->perf_freq.QuadPart ? runtime->perf_freq.QuadPart : 1);
        nsh_render_frame(runtime, elapsed_ms / 1000.0);
        nsh_present_frame(runtime);
        runtime->frame_count += 1;
        if (elapsed_ms >= (double)(duration_ms > 100 ? duration_ms : 1200)) {
            runtime->running = 0;
        }
        Sleep(16);
    }
    runtime->checksum = nsh_checksum_pixels(runtime);
    if (elapsed_ms > 0.0) {
        runtime->average_fps_x100 = (int)(((double)runtime->frame_count * 100000.0) / elapsed_ms);
    }
    snprintf(
        runtime->signature,
        sizeof(runtime->signature),
        "native-sculpt:%dx%d:r%d:i%d:h%d:p%d:f%d:c%d",
        runtime->width,
        runtime->height,
        runtime->radius,
        runtime->intensity,
        runtime->hardness,
        runtime->target_polys,
        runtime->frame_count,
        runtime->checksum
    );
    nsh_write_bmp(capture_bmp_path, runtime);
    if (runtime->hwnd && IsWindow(runtime->hwnd)) {
        DestroyWindow(runtime->hwnd);
    }
    UnregisterClassA(wc.lpszClassName, instance);
    return 1;
}

NSH_EXPORT int native_sculpt_runtime_frame_count(void* runtime_handle) {
    NativeSculptRuntime* runtime = (NativeSculptRuntime*)runtime_handle;
    return runtime ? runtime->frame_count : 0;
}

NSH_EXPORT int native_sculpt_runtime_message_count(void* runtime_handle) {
    NativeSculptRuntime* runtime = (NativeSculptRuntime*)runtime_handle;
    return runtime ? runtime->message_count : 0;
}

NSH_EXPORT int native_sculpt_runtime_mouse_move_count(void* runtime_handle) {
    NativeSculptRuntime* runtime = (NativeSculptRuntime*)runtime_handle;
    return runtime ? runtime->mouse_move_count : 0;
}

NSH_EXPORT int native_sculpt_runtime_last_brush_x(void* runtime_handle) {
    NativeSculptRuntime* runtime = (NativeSculptRuntime*)runtime_handle;
    return runtime ? runtime->last_brush_x : 0;
}

NSH_EXPORT int native_sculpt_runtime_last_brush_y(void* runtime_handle) {
    NativeSculptRuntime* runtime = (NativeSculptRuntime*)runtime_handle;
    return runtime ? runtime->last_brush_y : 0;
}

NSH_EXPORT int native_sculpt_runtime_average_fps_x100(void* runtime_handle) {
    NativeSculptRuntime* runtime = (NativeSculptRuntime*)runtime_handle;
    return runtime ? runtime->average_fps_x100 : 0;
}

NSH_EXPORT int native_sculpt_runtime_checksum(void* runtime_handle) {
    NativeSculptRuntime* runtime = (NativeSculptRuntime*)runtime_handle;
    return runtime ? runtime->checksum : 0;
}

NSH_EXPORT const char* native_sculpt_runtime_signature(void* runtime_handle) {
    NativeSculptRuntime* runtime = (NativeSculptRuntime*)runtime_handle;
    return runtime ? runtime->signature : "";
}

NSH_EXPORT void native_sculpt_runtime_destroy(void* runtime_handle) {
    NativeSculptRuntime* runtime = (NativeSculptRuntime*)runtime_handle;
    if (!runtime) {
        return;
    }
    free(runtime->pixels);
    free(runtime);
}

#else
void* native_sculpt_runtime_create(
    int width,
    int height,
    int radius,
    int intensity,
    int hardness,
    int target_polys,
    const char* title
) {
    (void)width;
    (void)height;
    (void)radius;
    (void)intensity;
    (void)hardness;
    (void)target_polys;
    (void)title;
    return NULL;
}

int native_sculpt_runtime_run(void* runtime_handle, int duration_ms, const char* capture_bmp_path) {
    (void)runtime_handle;
    (void)duration_ms;
    (void)capture_bmp_path;
    return 0;
}

int native_sculpt_runtime_frame_count(void* runtime_handle) { (void)runtime_handle; return 0; }
int native_sculpt_runtime_message_count(void* runtime_handle) { (void)runtime_handle; return 0; }
int native_sculpt_runtime_mouse_move_count(void* runtime_handle) { (void)runtime_handle; return 0; }
int native_sculpt_runtime_last_brush_x(void* runtime_handle) { (void)runtime_handle; return 0; }
int native_sculpt_runtime_last_brush_y(void* runtime_handle) { (void)runtime_handle; return 0; }
int native_sculpt_runtime_average_fps_x100(void* runtime_handle) { (void)runtime_handle; return 0; }
int native_sculpt_runtime_checksum(void* runtime_handle) { (void)runtime_handle; return 0; }
const char* native_sculpt_runtime_signature(void* runtime_handle) { (void)runtime_handle; return ""; }
void native_sculpt_runtime_destroy(void* runtime_handle) { (void)runtime_handle; }
#endif
