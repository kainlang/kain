// ============================================================================
//  kain_host_win32.c — Win32 GDI host backend (extracted from ui_host_adapter.c)
// ============================================================================
//  Implements the kainHostVTable for the Win32 platform using GDI:
//    - RegisterClassA("KainWin32UI") with CS_OWNDC
//    - CreateDIBSection top-down 32-bit DIB framebuffer
//    - PeekMessage/TranslateMessage/DispatchMessage event pump
//    - BitBlt present via WM_PAINT
//    - DPI scaling via SetProcessDpiAwarenessContext + GetDeviceCaps
//    - OS input events bridged to the universal input system
//
//  Part of Phase 1 C substrate extraction (P1-C-012). This file was
//  extracted from ui_host_adapter.c. The abi_ui_* ABI surface in
//  ui_host_adapter.c remains unchanged; it delegates to this file
//  internally through the kainHostVTable dispatch and helper functions.
// ============================================================================

#include "kain_host.h"
#include "kain_surface.h"
#include "ui_system_internal.h"
#include "../../include/ui_renderer.h"
#include "../../include/ui_layout.h"
#include "../../include/input_system.h"

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
//  Win32 host state — opaque outside this file
// ============================================================================

typedef struct KainWin32HostState {
    HWND      hwnd;
    int       width;
    int       height;
    int       running;
    int       initialized;
    uint8_t*  framebuffer;
    int       fb_stride;          // stride in bytes (width * 4)
    HDC       hdc_buffer;
    HBITMAP   hbitmap;
    int64_t   session_id;
    int64_t   input_session_id;
    float     dpi_scale;
} KainWin32HostState;

// ============================================================================
//  Win32 virtual-key → Kain key code string
// ============================================================================

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

// ============================================================================
//  WNDPROC — Window procedure (bridges OS events → input system)
// ============================================================================

static LRESULT CALLBACK kain_win32_ui_window_proc(
    HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
    KainWin32HostState* host =
        (KainWin32HostState*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    if (msg == WM_NCCREATE) {
        CREATESTRUCTA* cs = (CREATESTRUCTA*)l_param;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, w_param, l_param);
    }

    // ── Input event bridge ───────────────────────────────────────
    if (host && host->input_session_id > 0) {
        int64_t isid = host->input_session_id;

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
                double delta = (double)(short)HIWORD(w_param) /
                               (double)WHEEL_DELTA;
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
        case WM_SIZE: {
            if (!host) return 0;

            int new_w = LOWORD(l_param);
            int new_h = HIWORD(l_param);

            if (host->hbitmap) {
                HBITMAP temp_bmp = CreateBitmap(1, 1, 1, 1, NULL);
                SelectObject(host->hdc_buffer, temp_bmp);
                DeleteObject(host->hbitmap);
                host->hbitmap = temp_bmp;
                host->framebuffer = NULL;
            }

            BITMAPINFO bmi = {0};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = new_w;
            bmi.bmiHeader.biHeight = -new_h;  // top-down
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            HDC hdc_screen = GetDC(NULL);
            host->hbitmap = CreateDIBSection(hdc_screen, &bmi,
                DIB_RGB_COLORS, (void**)&host->framebuffer, NULL, 0);
            ReleaseDC(NULL, hdc_screen);

            if (host->hbitmap && host->hdc_buffer) {
                SelectObject(host->hdc_buffer, host->hbitmap);
            }

            if (host->framebuffer) {
                memset(host->framebuffer, 0, (size_t)new_w * new_h * 4);
            }

            host->width = new_w;
            host->height = new_h;
            host->fb_stride = new_w * 4;

            return 0;
        }
        case WM_ERASEBKGND:
            return 1;  // Don't erase, we paint everything
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (hdc && host && host->hdc_buffer) {
                BitBlt(hdc, 0, 0, host->width, host->height,
                       host->hdc_buffer, 0, 0, SRCCOPY);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DPICHANGED: {
            RECT* suggested_rect = (RECT*)l_param;
            SetWindowPos(hwnd, NULL,
                suggested_rect->left,
                suggested_rect->top,
                suggested_rect->right - suggested_rect->left,
                suggested_rect->bottom - suggested_rect->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, w_param, l_param);
}

// ============================================================================
//  VTABLE IMPLEMENTATIONS
// ============================================================================

// ── backend_id ────────────────────────────────────────────────────
static const char* win32_backend_id(void) {
    return "winit";
}

// ── platform ──────────────────────────────────────────────────────
static kainHostPlatform win32_platform(void) {
    return KAIN_HOST_WIN32;
}

// ── window_create ─────────────────────────────────────────────────
static void* win32_window_create(const char* title, int width, int height) {
    KainWin32HostState* host =
        (KainWin32HostState*)calloc(1, sizeof(KainWin32HostState));
    if (!host) return NULL;

    host->width = width;
    host->height = height;
    host->running = 1;

    // Register window class
    WNDCLASSA wc = {0};
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = kain_win32_ui_window_proc;
    wc.hInstance     = GetModuleHandleA(NULL);
    wc.hCursor       = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "KainWin32UI";

    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        free(host);
        return NULL;
    }

    // Defensively load the V2 DPI API before window creation
    {
        typedef BOOL (WINAPI *SetProcessDpiAwarenessContext_fn)(
            DPI_AWARENESS_CONTEXT);
        HMODULE user32 = GetModuleHandleA("user32.dll");
        if (user32) {
            SetProcessDpiAwarenessContext_fn set_dpi_aware =
                (SetProcessDpiAwarenessContext_fn)
                GetProcAddress(user32, "SetProcessDpiAwarenessContext");
            if (set_dpi_aware) {
                set_dpi_aware(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            }
        }
    }

    // Create window at explicit position (CW_USEDEFAULT can produce
    // off-screen coordinates on high-DPI/multi-monitor systems)
    host->hwnd = CreateWindowExA(
        0, "KainWin32UI", title,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100,
        width, height,
        NULL, NULL,
        GetModuleHandleA(NULL), host);

    if (!host->hwnd) {
        free(host);
        return NULL;
    }

    // Get actual client rect — DPI scaling may produce different size
    RECT client_rect;
    GetClientRect(host->hwnd, &client_rect);
    int actual_w = client_rect.right - client_rect.left;
    int actual_h = client_rect.bottom - client_rect.top;
    if (actual_w <= 0) actual_w = width;
    if (actual_h <= 0) actual_h = height;
    host->width  = actual_w;
    host->height = actual_h;

    // Grab initial baseline DPI before WM_DPICHANGED fires
    HDC hdc_screen_dpi = GetDC(NULL);
    host->dpi_scale = (float)GetDeviceCaps(hdc_screen_dpi, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, hdc_screen_dpi);

    // Discard the stale DIB created by WM_SIZE during CreateWindowExA
    // (it wasn't selected into any DC because hdc_buffer didn't exist yet)
    if (host->hbitmap) {
        DeleteObject(host->hbitmap);
        host->hbitmap = NULL;
        host->framebuffer = NULL;
    }

    // Create the real framebuffer at actual client size
    HDC hdc_screen = GetDC(NULL);
    host->hdc_buffer = CreateCompatibleDC(hdc_screen);
    if (host->hdc_buffer) {
        BITMAPINFO bmi = {0};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = actual_w;
        bmi.bmiHeader.biHeight      = -actual_h;  // top-down DIB
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        host->hbitmap = CreateDIBSection(hdc_screen, &bmi,
            DIB_RGB_COLORS, (void**)&host->framebuffer, NULL, 0);
        if (host->hbitmap) {
            SelectObject(host->hdc_buffer, host->hbitmap);
            host->fb_stride = actual_w * 4;
            // CreateDIBSection returns UNDEFINED memory per MSDN
            if (host->framebuffer) {
                memset(host->framebuffer, 0,
                       (size_t)actual_w * actual_h * 4);
            }
        }
    }
    ReleaseDC(NULL, hdc_screen);

    UpdateWindow(host->hwnd);
    host->initialized = 1;
    return (void*)host;
}

// ── window_destroy ────────────────────────────────────────────────
static void win32_window_destroy(void* state) {
    KainWin32HostState* host = (KainWin32HostState*)state;
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

// ── window_set_title ──────────────────────────────────────────────
static void win32_window_set_title(void* state, const char* title) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    if (host && host->hwnd) {
        SetWindowTextA(host->hwnd, title);
    }
}

// ── window_set_size ───────────────────────────────────────────────
static void win32_window_set_size(void* state, int width, int height) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    if (host && host->hwnd) {
        SetWindowPos(host->hwnd, NULL, 0, 0, width, height,
                     SWP_NOMOVE | SWP_NOZORDER);
    }
}

// ── window_get_size ───────────────────────────────────────────────
static void win32_window_get_size(void* state, int* out_w, int* out_h) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    if (host) {
        if (out_w) *out_w = host->width;
        if (out_h) *out_h = host->height;
    }
}

// ── window_get_dpi ────────────────────────────────────────────────
static float win32_window_get_dpi(void* state) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    return host ? host->dpi_scale : 1.0f;
}

// ── pump_events ───────────────────────────────────────────────────
static void win32_pump_events(void* state) {
    KainWin32HostState* host = (KainWin32HostState*)state;
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

// ── should_close ──────────────────────────────────────────────────
static int win32_should_close(void* state) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    return (host && host->running) ? 0 : 1;
}

// ── get_framebuffer ───────────────────────────────────────────────
static uint32_t* win32_get_framebuffer(void* state, int* out_stride_elems) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    if (!host || !host->framebuffer) {
        if (out_stride_elems) *out_stride_elems = 0;
        return NULL;
    }
    if (out_stride_elems) {
        *out_stride_elems = host->fb_stride / 4;  // stride in uint32_t
    }
    return (uint32_t*)host->framebuffer;
}

// ── get_framebuffer_width ─────────────────────────────────────────
static int win32_get_framebuffer_width(void* state) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    return host ? host->width : 0;
}

// ── get_framebuffer_height ────────────────────────────────────────
static int win32_get_framebuffer_height(void* state) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    return host ? host->height : 0;
}

// ── present ───────────────────────────────────────────────────────
static void win32_present(void* state, void* session) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    if (!host || !host->hwnd || !host->framebuffer || !session) return;

    KainNativeUiSession* s = (KainNativeUiSession*)session;

    // 1. Resolve layout
    ui_layout_resolve(s);

    // 2. Render node tree into framebuffer
    ui_render_frame(s,
        (uint32_t*)host->framebuffer,
        host->width,
        host->height,
        host->fb_stride / 4);

    // 3. Trigger WM_PAINT to blit framebuffer → screen
    InvalidateRect(host->hwnd, NULL, FALSE);
}

// ── clipboard_set_text ────────────────────────────────────────────
static int win32_clipboard_set_text(void* state, const char* text) {
    (void)state;
    (void)text;
    // Clipboard not yet implemented in Phase 1
    return 0;
}

// ── clipboard_get_text ────────────────────────────────────────────
static int win32_clipboard_get_text(void* state, char* out, size_t cap) {
    (void)state;
    (void)out;
    (void)cap;
    // Clipboard not yet implemented in Phase 1
    return 0;
}

// ── set_cursor ────────────────────────────────────────────────────
static void win32_set_cursor(void* state, kainHostCursor cursor) {
    (void)state;
    LPCSTR id = IDC_ARROW;
    switch (cursor) {
        case KAIN_CURSOR_ARROW:     id = (LPCSTR)IDC_ARROW;     break;
        case KAIN_CURSOR_IBEAM:     id = (LPCSTR)IDC_IBEAM;     break;
        case KAIN_CURSOR_HAND:      id = (LPCSTR)IDC_HAND;      break;
        case KAIN_CURSOR_RESIZE_NS: id = (LPCSTR)IDC_SIZENS;    break;
        case KAIN_CURSOR_RESIZE_EW: id = (LPCSTR)IDC_SIZEWE;    break;
        case KAIN_CURSOR_WAIT:      id = (LPCSTR)IDC_WAIT;      break;
        default: break;
    }
    SetCursor(LoadCursorA(NULL, id));
}

// ── get_gpu_surface ───────────────────────────────────────────────
static void* win32_get_gpu_surface(void* state) {
    (void)state;
    // Software backend has no GPU surface
    return NULL;
}

// ============================================================================
//  EXPORTED VTABLE
// ============================================================================

const kainHostVTable kain_host_win32_vtable = {
    .backend_id              = win32_backend_id,
    .platform                = win32_platform,
    .window_create           = win32_window_create,
    .window_destroy          = win32_window_destroy,
    .window_set_title        = win32_window_set_title,
    .window_set_size         = win32_window_set_size,
    .window_get_size         = win32_window_get_size,
    .window_get_dpi          = win32_window_get_dpi,
    .pump_events             = win32_pump_events,
    .should_close            = win32_should_close,
    .get_framebuffer         = win32_get_framebuffer,
    .get_framebuffer_width   = win32_get_framebuffer_width,
    .get_framebuffer_height  = win32_get_framebuffer_height,
    .present                 = win32_present,
    .clipboard_set_text      = win32_clipboard_set_text,
    .clipboard_get_text      = win32_clipboard_get_text,
    .set_cursor              = win32_set_cursor,
    .get_gpu_surface         = win32_get_gpu_surface,
};

// ============================================================================
//  Win32-specific helpers — used by ui_host_adapter.c framebuffer accessors
// ============================================================================
//  These are NOT part of the vtable. They provide direct access to
//  Win32-specific fields for the abi_ui_framebuffer_* and
//  abi_ui_invalidate_window functions that still live in ui_host_adapter.c.

uint32_t* kain_win32_framebuffer_ptr(void* state) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    return host ? (uint32_t*)host->framebuffer : NULL;
}

int kain_win32_framebuffer_width(void* state) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    return host ? host->width : 0;
}

int kain_win32_framebuffer_height(void* state) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    return host ? host->height : 0;
}

int kain_win32_framebuffer_stride_elems(void* state) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    return host ? (host->fb_stride / 4) : 0;
}

void* kain_win32_hwnd(void* state) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    return host ? (void*)host->hwnd : NULL;
}

int kain_win32_is_running(void* state) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    return host ? host->running : 0;
}

int64_t kain_win32_input_session_id(void* state) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    return host ? host->input_session_id : 0;
}

void kain_win32_set_session_id(void* state, int64_t session_id) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    if (host) host->session_id = session_id;
}

void kain_win32_set_input_session_id(void* state, int64_t input_sid) {
    KainWin32HostState* host = (KainWin32HostState*)state;
    if (host) host->input_session_id = input_sid;
}

// ============================================================================
//  Host dispatch
// ============================================================================

const kainHostVTable* kain_host_get(kainHostPlatform platform) {
    switch (platform) {
        case KAIN_HOST_WIN32: return &kain_host_win32_vtable;
        default:              return NULL;
    }
}

const kainHostVTable* kain_host_native(void) {
    return kain_host_get(kain_host_current_platform());
}

kainHostPlatform kain_host_current_platform(void) {
#ifdef _WIN32
    return KAIN_HOST_WIN32;
#elif defined(__APPLE__)
    return KAIN_HOST_MACOS;
#elif defined(__linux__)
    // Heuristic — true detection requires display server query
    return KAIN_HOST_X11;
#else
    return KAIN_HOST_WASM;
#endif
}

const char* kain_host_platform_name(kainHostPlatform p) {
    switch (p) {
        case KAIN_HOST_WIN32:   return "win32";
        case KAIN_HOST_X11:     return "x11";
        case KAIN_HOST_WAYLAND: return "wayland";
        case KAIN_HOST_MACOS:   return "macos";
        case KAIN_HOST_WASM:    return "wasm";
        default:                return "unknown";
    }
}

#else  /* !_WIN32 */

// ============================================================================
//  Non-Windows stubs — compile but return NULL
// ============================================================================

const kainHostVTable kain_host_win32_vtable = {0};

uint32_t* kain_win32_framebuffer_ptr(void* state)      { (void)state; return NULL; }
int       kain_win32_framebuffer_width(void* state)      { (void)state; return 0; }
int       kain_win32_framebuffer_height(void* state)     { (void)state; return 0; }
int       kain_win32_framebuffer_stride_elems(void* state) { (void)state; return 0; }
void*     kain_win32_hwnd(void* state)                   { (void)state; return NULL; }
int       kain_win32_is_running(void* state)             { (void)state; return 0; }
int64_t   kain_win32_input_session_id(void* state)       { (void)state; return 0; }
void      kain_win32_set_session_id(void* state, int64_t sid)       { (void)state; (void)sid; }
void      kain_win32_set_input_session_id(void* state, int64_t sid) { (void)state; (void)sid; }

const kainHostVTable* kain_host_get(kainHostPlatform platform) {
    (void)platform; return NULL;
}
const kainHostVTable* kain_host_native(void) { return NULL; }
kainHostPlatform kain_host_current_platform(void) { return KAIN_HOST_UNKNOWN; }
const char* kain_host_platform_name(kainHostPlatform p) {
    (void)p; return "unknown";
}

#endif /* _WIN32 */
