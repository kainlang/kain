#include "ui_host_adapter.h"
#include "../../include/win32.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

typedef struct KainWin32UiHost {
    HWND hwnd;
    int width;
    int height;
    int running;
    int initialized;
    uint8_t* framebuffer;
    int fb_stride;
    HDC hdc_buffer;
    HBITMAP hbitmap;
} KainWin32UiHost;

static LRESULT CALLBACK kain_win32_ui_window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    KainWin32UiHost* host = (KainWin32UiHost*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    if (msg == WM_NCCREATE) {
        CREATESTRUCTA* cs = (CREATESTRUCTA*)l_param;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, w_param, l_param);
    }

    switch (msg) {
        case WM_CLOSE:
            if (host) host->running = 0;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (host) host->running = 0;
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            if (host) {
                host->width = LOWORD(l_param);
                host->height = HIWORD(l_param);
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;  // Don't erase, we paint everything
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (hdc && host && host->framebuffer) {
                // Blit the framebuffer to the window
                HDC hdc_mem = CreateCompatibleDC(hdc);
                if (hdc_mem) {
                    HBITMAP old = (HBITMAP)SelectObject(hdc_mem, host->hbitmap);
                    BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
                           ps.rcPaint.right - ps.rcPaint.left,
                           ps.rcPaint.bottom - ps.rcPaint.top,
                           hdc_mem, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
                    SelectObject(hdc_mem, old);
                    DeleteDC(hdc_mem);
                }
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, w_param, l_param);
}

static KainWin32UiHost* win32_host_create(int width, int height) {
    KainWin32UiHost* host = (KainWin32UiHost*)calloc(1, sizeof(KainWin32UiHost));
    if (!host) return NULL;

    host->width = width;
    host->height = height;
    host->running = 1;

    // Register window class
    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = kain_win32_ui_window_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "KainWin32UI";

    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        free(host);
        return NULL;
    }

    // Create window
    host->hwnd = CreateWindowExA(
        0, "KainWin32UI", "Kain UI",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        NULL, NULL,
        GetModuleHandleA(NULL), host
    );

    if (!host->hwnd) {
        free(host);
        return NULL;
    }

    // Create framebuffer
    HDC hdc_screen = GetDC(NULL);
    host->hdc_buffer = CreateCompatibleDC(hdc_screen);
    if (host->hdc_buffer) {
        BITMAPINFO bmi = {0};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;  // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        host->hbitmap = CreateDIBSection(hdc_screen, &bmi, DIB_RGB_COLORS,
                                         (void**)&host->framebuffer, NULL, 0);
        if (host->hbitmap) {
            SelectObject(host->hdc_buffer, host->hbitmap);
            host->fb_stride = width * 4;
        }
    }
    ReleaseDC(NULL, hdc_screen);

    UpdateWindow(host->hwnd);
    host->initialized = 1;
    return host;
}

static void win32_host_destroy(KainWin32UiHost* host) {
    if (!host) return;
    if (host->hbitmap) {
        DeleteObject(host->hbitmap);
    }
    if (host->hdc_buffer) {
        DeleteDC(host->hdc_buffer);
    }
    if (host->hwnd && IsWindow(host->hwnd)) {
        DestroyWindow(host->hwnd);
    }
    free(host);
}

static void win32_host_render_framebuffer(KainWin32UiHost* host) {
    if (!host || !host->hwnd || !host->framebuffer) return;

    // Fill framebuffer with dark background
    int i;
    for (i = 0; i < host->width * host->height; i++) {
        ((uint32_t*)host->framebuffer)[i] = 0xFF1A1A24;  // Dark blue-gray ARGB
    }

    // Draw a colored gradient
    int y;
    for (y = 0; y < host->height; y++) {
        uint32_t* row = (uint32_t*)host->framebuffer + y * host->width;
        int x;
        for (x = 0; x < host->width; x++) {
            // Simple gradient based on position
            uint8_t r = (uint8_t)((x * 255) / host->width);
            uint8_t g = (uint8_t)((y * 255) / host->height);
            uint8_t b = (uint8_t)(((x + y) * 128) / (host->width + host->height));
            row[x] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }

    // Draw some text
    HDC hdc = GetDC(host->hwnd);
    if (hdc) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(240, 240, 255));

        // Title
        RECT title_rect = {20, 20, host->width - 20, 60};
        DrawTextA(hdc, "Kain UI Window (winit backend)", -1, &title_rect, DT_LEFT);

        // Subtitle
        RECT sub_rect = {20, 60, host->width - 20, 90};
        DrawTextA(hdc, "Rendered by Pure Kain :: std::ui", -1, &sub_rect, DT_LEFT);

        // Info
        RECT info_rect = {20, host->height - 40, host->width - 20, host->height - 10};
        char info[128];
        snprintf(info, sizeof(info), "Resolution: %dx%d", host->width, host->height);
        DrawTextA(hdc, info, -1, &info_rect, DT_LEFT);

        ReleaseDC(host->hwnd, hdc);
    }

    // Trigger WM_PAINT to blit framebuffer
    InvalidateRect(host->hwnd, NULL, FALSE);
}

static void win32_host_pump_messages(KainWin32UiHost* host) {
    if (!host) return;
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            host->running = 0;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

#endif  // _WIN32

static int64_t abi_ui_host_adapter_attach_passive(
    KainNativeUiSession* session,
    const char* resolved_backend_id
) {
    if (!session || !resolved_backend_id || !resolved_backend_id[0]) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    session->host_attached = 1;
    session->host_state = NULL;
    snprintf(session->host_backend, sizeof(session->host_backend), "%s", resolved_backend_id);
    return ABI_UI_OK;
}

int abi_ui_host_adapter_is_live_backend(const char* backend_id) {
    if (!backend_id) return 0;
    if (strcmp(backend_id, "winit") == 0) return 1;
    (void)backend_id;
    return 0;
}

int64_t abi_ui_host_adapter_attach(KainNativeUiSession* session, const char* backend_id) {
    if (!session || !backend_id || !backend_id[0]) {
        return ABI_UI_INVALID_ARGUMENT;
    }
    if (strcmp(backend_id, "auto") == 0) {
        return abi_ui_host_adapter_attach_passive(session, "software");
    }
    if (strcmp(backend_id, "headless") == 0 ||
        strcmp(backend_id, "memory") == 0 ||
        strcmp(backend_id, "software") == 0) {
        return abi_ui_host_adapter_attach_passive(session, backend_id);
    }
#ifdef _WIN32
    if (strcmp(backend_id, "winit") == 0) {
        KainWin32UiHost* win32_host = win32_host_create(
            (int)session->width,
            (int)session->height
        );
        if (!win32_host) {
            return ABI_UI_INVALID_ARGUMENT;
        }
        session->host_state = (void*)win32_host;
        session->host_attached = 1;
        snprintf(session->host_backend, sizeof(session->host_backend), "winit");
        return ABI_UI_OK;
    }
#endif
    return ABI_UI_INVALID_ARGUMENT;
}

int64_t abi_ui_host_adapter_pump(KainNativeUiSession* session) {
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
#ifdef _WIN32
    if (session->host_state && strcmp(session->host_backend, "winit") == 0) {
        KainWin32UiHost* win32_host = (KainWin32UiHost*)session->host_state;
        win32_host_pump_messages(win32_host);
        if (!win32_host->running) {
            session->host_should_close = 1;
        }
    }
#endif
    return ABI_UI_OK;
}

int64_t abi_ui_host_adapter_present(KainNativeUiSession* session) {
    if (!session) {
        return ABI_UI_INVALID_SESSION;
    }
#ifdef _WIN32
    if (session->host_state && strcmp(session->host_backend, "winit") == 0) {
        KainWin32UiHost* win32_host = (KainWin32UiHost*)session->host_state;
        win32_host_render_framebuffer(win32_host);
    }
#endif
    return ABI_UI_OK;
}

void abi_ui_host_adapter_shutdown(KainNativeUiSession* session) {
    if (!session) {
        return;
    }
#ifdef _WIN32
    if (session->host_state && strcmp(session->host_backend, "winit") == 0) {
        KainWin32UiHost* win32_host = (KainWin32UiHost*)session->host_state;
        win32_host_destroy(win32_host);
        session->host_state = NULL;
    }
#endif
    session->host_state = NULL;
}

int abi_ui_host_adapter_clipboard_set_text(KainNativeUiSession* session, const char* text) {
    if (!session) {
        return 0;
    }
    (void)text;
    return 0;
}

int abi_ui_host_adapter_clipboard_get_text(
    KainNativeUiSession* session,
    char* out_text,
    size_t out_text_cap
) {
    if (!session) {
        return 0;
    }
    (void)out_text;
    (void)out_text_cap;
    return 0;
}
