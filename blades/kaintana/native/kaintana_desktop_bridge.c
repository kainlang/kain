#define WIN32_LEAN_AND_MEAN
#include "kaintana_desktop_bridge.h"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define KAINTANA_DESKTOP_MAX_COMMANDS 2048
#define KAINTANA_DESKTOP_TEXT_CAPACITY 256

typedef enum KaintanaDesktopCommandKind {
    KAINTANA_DESKTOP_COMMAND_RECT = 1,
    KAINTANA_DESKTOP_COMMAND_TEXT = 2
} KaintanaDesktopCommandKind;

typedef struct KaintanaDesktopCommand {
    int kind;
    int x;
    int y;
    int width;
    int height;
    int red;
    int green;
    int blue;
    int alpha;
    char text[KAINTANA_DESKTOP_TEXT_CAPACITY];
} KaintanaDesktopCommand;

static KaintanaDesktopCommand g_commands[KAINTANA_DESKTOP_MAX_COMMANDS];
static int g_command_count = 0;
static int g_scene_active = 0;
static int g_frames_presented = 0;
static int g_last_error = 0;
static int g_width = 1280;
static int g_height = 720;
static int g_clear_red = 8;
static int g_clear_green = 14;
static int g_clear_blue = 24;
static char g_title[128] = "Kaintana Desktop Host";

static void kaintana_copy_text(char* destination, size_t destination_capacity, const char* source) {
    if (destination == NULL || destination_capacity == 0U) {
        return;
    }
    destination[0] = '\0';
    if (source == NULL) {
        return;
    }
    strncpy(destination, source, destination_capacity - 1U);
    destination[destination_capacity - 1U] = '\0';
}

static int kaintana_clamp_channel(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return value;
}

static COLORREF kaintana_rgb(int red, int green, int blue) {
    return RGB(
        kaintana_clamp_channel(red),
        kaintana_clamp_channel(green),
        kaintana_clamp_channel(blue)
    );
}

static int kaintana_scaled_value(int value, float scale) {
    const int scaled = (int)((float)value * scale);
    if (value > 0 && scaled < 1) {
        return 1;
    }
    return scaled;
}

static int kaintana_text_pixel_height(int encoded_size, float scale_y) {
    int size = encoded_size;
    if (size <= 0 || size > 96) {
        size = 16;
    }
    size = kaintana_scaled_value(size, scale_y);
    if (size < 10) {
        size = 10;
    }
    if (size > 56) {
        size = 56;
    }
    return size;
}

static HFONT kaintana_create_ui_font(int pixel_height) {
    const int weight = pixel_height >= 18 ? FW_SEMIBOLD : FW_NORMAL;
    return CreateFontA(
        -pixel_height,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        "Segoe UI"
    );
}

static void kaintana_render_scene(HDC hdc, int width, int height) {
    const float scale_x = g_width > 0 ? (float)width / (float)g_width : 1.0f;
    const float scale_y = g_height > 0 ? (float)height / (float)g_height : 1.0f;
    RECT background_rect;
    background_rect.left = 0;
    background_rect.top = 0;
    background_rect.right = width;
    background_rect.bottom = height;

    HBRUSH background_brush = CreateSolidBrush(kaintana_rgb(g_clear_red, g_clear_green, g_clear_blue));
    FillRect(hdc, &background_rect, background_brush);
    DeleteObject(background_brush);

    SetBkMode(hdc, TRANSPARENT);

    for (int index = 0; index < g_command_count; ++index) {
        const KaintanaDesktopCommand* command = &g_commands[index];
        if (command->kind == KAINTANA_DESKTOP_COMMAND_RECT) {
            RECT rect;
            rect.left = kaintana_scaled_value(command->x, scale_x);
            rect.top = kaintana_scaled_value(command->y, scale_y);
            rect.right = rect.left + kaintana_scaled_value(command->width, scale_x);
            rect.bottom = rect.top + kaintana_scaled_value(command->height, scale_y);
            HBRUSH brush = CreateSolidBrush(kaintana_rgb(command->red, command->green, command->blue));
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);
        } else if (command->kind == KAINTANA_DESKTOP_COMMAND_TEXT) {
            const int pixel_height = kaintana_text_pixel_height(command->alpha, scale_y);
            HFONT font = kaintana_create_ui_font(pixel_height);
            HGDIOBJ old_font = NULL;
            if (font != NULL) {
                old_font = SelectObject(hdc, font);
            }
            SetTextColor(hdc, kaintana_rgb(command->red, command->green, command->blue));
            TextOutA(
                hdc,
                kaintana_scaled_value(command->x, scale_x),
                kaintana_scaled_value(command->y, scale_y),
                command->text,
                (int)strlen(command->text)
            );
            if (font != NULL) {
                SelectObject(hdc, old_font);
                DeleteObject(font);
            }
        }
    }
}

static int kaintana_write_scene_bmp(const char* path) {
    if (path == NULL || path[0] == '\0') {
        return 21;
    }

    BITMAPINFO bitmap_info;
    ZeroMemory(&bitmap_info, sizeof(bitmap_info));
    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = g_width;
    bitmap_info.bmiHeader.biHeight = -g_height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    HDC screen_dc = GetDC(NULL);
    if (screen_dc == NULL) {
        return 22;
    }

    void* pixels = NULL;
    HBITMAP dib = CreateDIBSection(screen_dc, &bitmap_info, DIB_RGB_COLORS, &pixels, NULL, 0U);
    if (dib == NULL || pixels == NULL) {
        ReleaseDC(NULL, screen_dc);
        return 23;
    }

    HDC memory_dc = CreateCompatibleDC(screen_dc);
    if (memory_dc == NULL) {
        DeleteObject(dib);
        ReleaseDC(NULL, screen_dc);
        return 24;
    }

    HGDIOBJ old_object = SelectObject(memory_dc, dib);
    kaintana_render_scene(memory_dc, g_width, g_height);
    SelectObject(memory_dc, old_object);

    const DWORD pixel_bytes = (DWORD)(g_width * g_height * 4);
    BITMAPFILEHEADER file_header;
    file_header.bfType = 0x4D42U;
    file_header.bfSize = (DWORD)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + pixel_bytes);
    file_header.bfReserved1 = 0U;
    file_header.bfReserved2 = 0U;
    file_header.bfOffBits = (DWORD)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER));

    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        DeleteDC(memory_dc);
        DeleteObject(dib);
        ReleaseDC(NULL, screen_dc);
        return 25;
    }

    fwrite(&file_header, sizeof(file_header), 1U, file);
    fwrite(&bitmap_info.bmiHeader, sizeof(bitmap_info.bmiHeader), 1U, file);
    fwrite(pixels, pixel_bytes, 1U, file);
    fclose(file);

    DeleteDC(memory_dc);
    DeleteObject(dib);
    ReleaseDC(NULL, screen_dc);
    return 0;
}

static LRESULT CALLBACK kaintana_desktop_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    (void)lparam;

    if (message == WM_ERASEBKGND) {
        return 1;
    }

    if (message == WM_SIZE) {
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    if (message == WM_PAINT) {
        PAINTSTRUCT paint;
        HDC hdc = BeginPaint(hwnd, &paint);
        RECT client_rect;
        GetClientRect(hwnd, &client_rect);
        const int client_width = client_rect.right - client_rect.left;
        const int client_height = client_rect.bottom - client_rect.top;
        if (client_width > 0 && client_height > 0) {
            HDC memory_dc = CreateCompatibleDC(hdc);
            HBITMAP bitmap = CreateCompatibleBitmap(hdc, client_width, client_height);
            if (memory_dc != NULL && bitmap != NULL) {
                HGDIOBJ old_object = SelectObject(memory_dc, bitmap);
                kaintana_render_scene(memory_dc, client_width, client_height);
                BitBlt(hdc, 0, 0, client_width, client_height, memory_dc, 0, 0, SRCCOPY);
                SelectObject(memory_dc, old_object);
            } else {
                kaintana_render_scene(hdc, client_width, client_height);
            }
            if (bitmap != NULL) {
                DeleteObject(bitmap);
            }
            if (memory_dc != NULL) {
                DeleteDC(memory_dc);
            }
        }
        EndPaint(hwnd, &paint);
        return 0;
    }

    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}

int kaintana_native_desktop_probe(void) {
    return 1;
}

int kaintana_native_desktop_scene_active(void) {
    return g_scene_active;
}

int kaintana_native_desktop_reset(void) {
    g_command_count = 0;
    g_scene_active = 0;
    g_frames_presented = 0;
    g_last_error = 0;
    return 0;
}

int kaintana_native_desktop_begin_scene(
    const char* title,
    int width,
    int height,
    int clear_red,
    int clear_green,
    int clear_blue
) {
    kaintana_native_desktop_reset();
    g_scene_active = 1;
    g_width = width > 64 ? width : 64;
    g_height = height > 64 ? height : 64;
    g_clear_red = kaintana_clamp_channel(clear_red);
    g_clear_green = kaintana_clamp_channel(clear_green);
    g_clear_blue = kaintana_clamp_channel(clear_blue);
    kaintana_copy_text(g_title, sizeof(g_title), title);
    return 0;
}

int kaintana_native_desktop_push_rect(
    int x,
    int y,
    int width,
    int height,
    int red,
    int green,
    int blue,
    int alpha
) {
    if (!g_scene_active) {
        return 0;
    }
    if (g_command_count >= KAINTANA_DESKTOP_MAX_COMMANDS) {
        g_last_error = 1;
        return 31;
    }

    KaintanaDesktopCommand* command = &g_commands[g_command_count];
    command->kind = KAINTANA_DESKTOP_COMMAND_RECT;
    command->x = x;
    command->y = y;
    command->width = width;
    command->height = height;
    command->red = kaintana_clamp_channel(red);
    command->green = kaintana_clamp_channel(green);
    command->blue = kaintana_clamp_channel(blue);
    command->alpha = kaintana_clamp_channel(alpha);
    command->text[0] = '\0';
    g_command_count += 1;
    return 0;
}

int kaintana_native_desktop_push_text(
    const char* text,
    int x,
    int y,
    int red,
    int green,
    int blue,
    int alpha
) {
    if (!g_scene_active) {
        return 0;
    }
    if (g_command_count >= KAINTANA_DESKTOP_MAX_COMMANDS) {
        g_last_error = 1;
        return 32;
    }

    KaintanaDesktopCommand* command = &g_commands[g_command_count];
    command->kind = KAINTANA_DESKTOP_COMMAND_TEXT;
    command->x = x;
    command->y = y;
    command->width = 0;
    command->height = 0;
    command->red = kaintana_clamp_channel(red);
    command->green = kaintana_clamp_channel(green);
    command->blue = kaintana_clamp_channel(blue);
    command->alpha = kaintana_clamp_channel(alpha);
    kaintana_copy_text(command->text, sizeof(command->text), text);
    g_command_count += 1;
    return 0;
}

int kaintana_native_desktop_run_window(int frame_budget) {
    if (!g_scene_active) {
        return 40;
    }

    HINSTANCE instance = GetModuleHandleA(NULL);
    const char* class_name = "KaintanaDesktopHostWindowClass";

    WNDCLASSA window_class;
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = kaintana_desktop_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = class_name;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassA(&window_class);

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT outer = { 0, 0, g_width, g_height };
    AdjustWindowRect(&outer, style, FALSE);

    HWND hwnd = CreateWindowExA(
        0,
        class_name,
        g_title,
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        outer.right - outer.left,
        outer.bottom - outer.top,
        NULL,
        NULL,
        instance,
        NULL
    );
    if (hwnd == NULL) {
        g_last_error = 2;
        return 41;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);

    int target_frames = frame_budget > 0 ? frame_budget : 180;
    int running = 1;
    MSG message;
    ULONGLONG next_tick = GetTickCount64();
    while (running && g_frames_presented < target_frames) {
        while (PeekMessageA(&message, NULL, 0U, 0U, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running = 0;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        if (!running) {
            break;
        }
        {
            const ULONGLONG now = GetTickCount64();
            if (now >= next_tick) {
                g_frames_presented += 1;
                next_tick = now + 16ULL;
            } else {
                Sleep(1U);
            }
        }
    }

    DestroyWindow(hwnd);
    return 0;
}

int kaintana_native_desktop_command_count(void) {
    return g_command_count;
}

int kaintana_native_desktop_frames_presented(void) {
    return g_frames_presented;
}

int kaintana_native_desktop_write_report(const char* path) {
    if (path == NULL || path[0] == '\0') {
        return 50;
    }
    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        return 51;
    }
    fprintf(file, "backend=desktop\n");
    fprintf(file, "title=%s\n", g_title);
    fprintf(file, "width=%d\n", g_width);
    fprintf(file, "height=%d\n", g_height);
    fprintf(file, "commands=%d\n", g_command_count);
    fprintf(file, "frames=%d\n", g_frames_presented);
    fprintf(file, "last_error=%s\n", g_last_error == 0 ? "ok" : "command_overflow");
    fclose(file);
    return 0;
}

int kaintana_native_desktop_write_bmp(const char* path) {
    return kaintana_write_scene_bmp(path);
}
