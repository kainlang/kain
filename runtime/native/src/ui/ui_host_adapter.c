#include "ui_host_adapter.h"
#include "ui_system_internal.h"
#include "../../include/win32.h"
#include "../../include/ui_renderer.h"
#include "../../include/ui_layout.h"
#include "../../include/input_system.h"

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
    int64_t session_id;
    int64_t input_session_id;
} KainWin32UiHost;

// ── Win32 virtual-key → Kain key code string ────────────────────────
// Maps common VK_* codes to human-readable key names for the input system.
static const char* win32_vk_to_key_string(WPARAM vk) {
    switch (vk) {
        case VK_RETURN:  return "Enter";
        case VK_ESCAPE:  return "Escape";
        case VK_BACK:    return "Backspace";
        case VK_TAB:     return "Tab";
        case VK_SPACE:   return "Space";
        case VK_LEFT:    return "ArrowLeft";
        case VK_UP:      return "ArrowUp";
        case VK_RIGHT:   return "ArrowRight";
        case VK_DOWN:    return "ArrowDown";
        case VK_SHIFT:   return "Shift";
        case VK_CONTROL: return "Control";
        case VK_MENU:    return "Alt";
        case VK_DELETE:  return "Delete";
        case VK_HOME:    return "Home";
        case VK_END:     return "End";
        case VK_PRIOR:   return "PageUp";
        case VK_NEXT:    return "PageDown";
        case VK_F1:      return "F1";
        case VK_F2:      return "F2";
        case VK_F3:      return "F3";
        case VK_F4:      return "F4";
        case VK_F5:      return "F5";
        case VK_F6:      return "F6";
        case VK_F7:      return "F7";
        case VK_F8:      return "F8";
        case VK_F9:      return "F9";
        case VK_F10:     return "F10";
        case VK_F11:     return "F11";
        case VK_F12:     return "F12";
        default: {
            static char buf[8];
            char ch = (char)MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_CHAR);
            if (ch >= 32 && ch < 127) {
                snprintf(buf, sizeof(buf), "%c", ch);
            } else {
                snprintf(buf, sizeof(buf), "VK%d", (int)vk);
            }
            return buf;
        }
    }
}

static LRESULT CALLBACK kain_win32_ui_window_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    KainWin32UiHost* host = (KainWin32UiHost*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    if (msg == WM_NCCREATE) {
        CREATESTRUCTA* cs = (CREATESTRUCTA*)l_param;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, w_param, l_param);
    }

    // ── Input event bridge: translate OS events → universal input format ──
    if (host && host->input_session_id > 0) {
        int64_t isid = host->input_session_id;
        int x = (int)(short)LOWORD(l_param);
        int y = (int)(short)HIWORD(l_param);

        switch (msg) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                abi_input_push_event(isid, "keyboard", "", "key_down",
                    win32_vk_to_key_string(w_param), 1.0, "", 1.0);
                break;
            case WM_KEYUP:
            case WM_SYSKEYUP:
                abi_input_push_event(isid, "keyboard", "", "key_up",
                    win32_vk_to_key_string(w_param), 0.0, "", 1.0);
                break;
            case WM_CHAR:
                if (w_param >= 32 && w_param != 127) {
                    char text[2] = { (char)w_param, '\0' };
                    abi_input_push_event(isid, "keyboard", "", "text",
                        "", 1.0, text, 1.0);
                }
                break;
            case WM_LBUTTONDOWN:
                abi_input_push_event(isid, "pointer", "", "pointer_down",
                    "left", 1.0, "", 1.0);
                break;
            case WM_LBUTTONUP:
                abi_input_push_event(isid, "pointer", "", "pointer_up",
                    "left", 0.0, "", 1.0);
                break;
            case WM_RBUTTONDOWN:
                abi_input_push_event(isid, "pointer", "", "pointer_down",
                    "right", 1.0, "", 1.0);
                break;
            case WM_RBUTTONUP:
                abi_input_push_event(isid, "pointer", "", "pointer_up",
                    "right", 0.0, "", 1.0);
                break;
            case WM_MBUTTONDOWN:
                abi_input_push_event(isid, "pointer", "", "pointer_down",
                    "middle", 1.0, "", 1.0);
                break;
            case WM_MBUTTONUP:
                abi_input_push_event(isid, "pointer", "", "pointer_up",
                    "middle", 0.0, "", 1.0);
                break;
            case WM_MOUSEMOVE:
                abi_input_push_event(isid, "pointer", "", "pointer_move",
                    "", 0.0, "", 1.0);
                break;
            case WM_MOUSEWHEEL: {
                double delta = (double)(short)HIWORD(w_param) / (double)WHEEL_DELTA;
                abi_input_push_event(isid, "pointer", "", "axis",
                    "wheel", delta, "", 1.0);
                break;
            }
        }
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

static void win32_host_render_framebuffer(KainWin32UiHost* host, KainNativeUiSession* session) {
    if (!host || !host->hwnd || !host->framebuffer || !session) return;

    // ── Universal render path ──────────────────────────────────────
    // 1. Resolve layout: compute pixel rects from styles + parent tree
    ui_layout_resolve(session);

    // 2. Render the node tree into the framebuffer
    ui_render_frame(
        session,
        (uint32_t*)host->framebuffer,
        host->width,
        host->height,
        host->fb_stride / 4  // stride in uint32_t elements
    );

    // 3. Trigger WM_PAINT to blit framebuffer to screen
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
        win32_host->session_id = session->id;
        // Create a companion input session so OS events flow into the universal input system
        win32_host->input_session_id = abi_input_session_create(session->app_name);
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
        // Process pending input events for this frame
        if (win32_host->input_session_id > 0) {
            double delta = session->last_delta_ms > 0.0 ? session->last_delta_ms : 16.67;
            abi_input_begin_frame(win32_host->input_session_id, delta);
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
        // Pass session to renderer so it can access the node tree
        win32_host_render_framebuffer(win32_host, session);
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
        if (win32_host->input_session_id > 0) {
            abi_input_session_destroy(win32_host->input_session_id);
        }
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
