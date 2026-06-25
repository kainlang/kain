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
    float dpi_scale;
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
        case WM_SIZE: {
            if (!host) return 0;

            int new_w = LOWORD(l_param);
            int new_h = HIWORD(l_param);

            if (host->hbitmap) {
                // Replace with a temporary 1x1 bitmap so we can delete the old DIB.
                // GetStockObject has no DEFAULT_BITMAP, so we create a tiny temp.
                HBITMAP temp_bmp = CreateBitmap(1, 1, 1, 1, NULL);
                SelectObject(host->hdc_buffer, temp_bmp);
                DeleteObject(host->hbitmap);
                host->hbitmap = temp_bmp;
                host->framebuffer = NULL;
            }

            BITMAPINFO bmi = {0};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = new_w;
            bmi.bmiHeader.biHeight = -new_h; // top-down
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            HDC hdc_screen = GetDC(NULL);
            host->hbitmap = CreateDIBSection(hdc_screen, &bmi, DIB_RGB_COLORS, (void**)&host->framebuffer, NULL, 0);
            ReleaseDC(NULL, hdc_screen);

            if (host->hbitmap && host->hdc_buffer) {
                SelectObject(host->hdc_buffer, host->hbitmap);
            }

            // Clear the garbage memory immediately
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
                // ── Full-rect BitBlt ──────────────────────────────────────────
                // Always blit the ENTIRE DIB framebuffer to the ENTIRE window
                // client area. This matches the proven pattern from the C demos
                // (cosmic_dashboard.c, path_a_full_pipeline.c) that render
                // correctly.
                //
                // Why full-rect and not ps.rcPaint:
                // 1. CS_OWNDC: BeginPaint does NOT set the clipping region to the
                //    invalid area. Using ps.rcPaint only blits the invalid rect,
                //    leaving perfectly valid-but-unpainted regions to show the
                //    default COLOR_WINDOW background (grey).
                // 2. CreateDIBSection returns UNDEFINED memory (per MSDN). On
                //    first window show, the initial DIB contents are not
                //    guaranteed to be zeroed. Full-rect blit ensures the
                //    renderer's cleared framebuffer (0xFF1A1A24) always reaches
                //    the screen.
                // 3. InvalidateRect(..., FALSE) marks regions invalid WITHOUT
                //    erasing background. If ps.rcPaint doesn't cover the full
                //    client (e.g. during initial show or resize), partial blit
                //    leaves non-invalidated regions grey.
                //
                // Do NOT create a temp DC — GDI cannot SelectObject the same
                // bitmap into two DCs simultaneously. The second SelectObject
                // silently fails, leaving a default 1x1 monochrome bitmap
                // selected instead of the DIB.
                BitBlt(hdc, 0, 0, host->width, host->height,
                       host->hdc_buffer, 0, 0, SRCCOPY);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DPICHANGED: {
            // Windows hands you the perfect RECT for the new monitor's scale
            RECT* suggested_rect = (RECT*)l_param;
            SetWindowPos(hwnd,
                         NULL,
                         suggested_rect->left,
                         suggested_rect->top,
                         suggested_rect->right - suggested_rect->left,
                         suggested_rect->bottom - suggested_rect->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            // SetWindowPos automatically triggers WM_SIZE, which reallocates the DIBSection
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

    // Defensively load the V2 DPI API before window creation
    {
        typedef BOOL(WINAPI *SetProcessDpiAwarenessContext_fn)(DPI_AWARENESS_CONTEXT);
        HMODULE user32 = GetModuleHandleA("user32.dll");
        if (user32) {
            SetProcessDpiAwarenessContext_fn set_dpi_aware =
                (SetProcessDpiAwarenessContext_fn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
            if (set_dpi_aware) {
                set_dpi_aware(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            }
        }
    }

    // Create window — use explicit position, CW_USEDEFAULT can produce
    // off-screen coordinates on high-DPI/multi-monitor systems.
    host->hwnd = CreateWindowExA(
        0, "KainWin32UI", "Kain UI",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100,
        width, height,
        NULL, NULL,
        GetModuleHandleA(NULL), host
    );

    if (!host->hwnd) {
        free(host);
        return NULL;
    }

    // Get actual client rect — DPI scaling may produce a different size
    // than requested. The DIB must match the client area exactly.
    RECT client_rect;
    GetClientRect(host->hwnd, &client_rect);
    int actual_w = client_rect.right - client_rect.left;
    int actual_h = client_rect.bottom - client_rect.top;
    if (actual_w <= 0) actual_w = width;
    if (actual_h <= 0) actual_h = height;
    host->width = actual_w;
    host->height = actual_h;

    // Grab the initial baseline DPI before WM_DPICHANGED ever fires
    HDC hdc_screen2 = GetDC(NULL);
    host->dpi_scale = (float)GetDeviceCaps(hdc_screen2, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, hdc_screen2);

    // Create framebuffer at actual client size
    // NOTE: During CreateWindowExA above, WM_SIZE fires and creates a DIB
    // at the client size. That DIB is NOT selected into any DC because
    // hdc_buffer didn't exist yet. We must delete the stale DIB to avoid
    // leaking it, then create the real DIB and clear it immediately since
    // CreateDIBSection memory is UNDEFINED per MSDN.
    if (host->hbitmap) {
        DeleteObject(host->hbitmap);
        host->hbitmap = NULL;
        host->framebuffer = NULL;
    }

    HDC hdc_screen = GetDC(NULL);
    host->hdc_buffer = CreateCompatibleDC(hdc_screen);
    if (host->hdc_buffer) {
        BITMAPINFO bmi = {0};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = actual_w;
        bmi.bmiHeader.biHeight = -actual_h;  // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        host->hbitmap = CreateDIBSection(hdc_screen, &bmi, DIB_RGB_COLORS,
                                         (void**)&host->framebuffer, NULL, 0);
        if (host->hbitmap) {
            SelectObject(host->hdc_buffer, host->hbitmap);
            host->fb_stride = actual_w * 4;
            // ── Clear the DIB immediately ──────────────────────────────
            // CreateDIBSection returns UNDEFINED pixel memory per MSDN.
            // On most Windows versions the pages are zero-filled, but this
            // is NOT guaranteed. Explicitly clear to black so the first
            // WM_PAINT (from UpdateWindow below) blits a defined state to
            // the screen, not garbage.
            if (host->framebuffer) {
                memset(host->framebuffer, 0, (size_t)actual_w * actual_h * 4);
            }
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
    if (strcmp(backend_id, "vulkan") == 0) return 1;
    if (strcmp(backend_id, "d3d12") == 0) return 1;
    if (strcmp(backend_id, "webgpu") == 0) return 1;
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
        // Sync session dimensions with actual DPI-scaled client rect.
        // win32_host_create uses GetClientRect after CreateWindowExA to get the
        // real pixel dimensions (which may differ from the requested size on
        // high-DPI displays). If we don't sync these, the layout engine and
        // renderer disagree on the coordinate space, causing pixel offset
        // overflows in ui_draw_fill_rect and crashes in ui_render_frame.
        session->width = win32_host->width;
        session->height = win32_host->height;
        // Push DPI scale from host to session for the renderer
        session->dpi_scale = (double)win32_host->dpi_scale;
        return ABI_UI_OK;
    }
#endif
    if (strcmp(backend_id, "vulkan") == 0) {
        const KainComponentSurface* surface =
            kain_component_surface_resolve("vulkan");
        if (surface == NULL) return ABI_UI_INVALID_ARGUMENT;
        int64_t vulkan_session = surface->session_create(
            session->window_title, session->width, session->height);
        if (vulkan_session < 0) return ABI_UI_CAPACITY_EXCEEDED;
        session->host_backend[0] = '\0';
        snprintf(session->host_backend, sizeof(session->host_backend), "vulkan");
        session->component_surface = surface;
        session->component_session_id = vulkan_session;
        session->host_attached = 1;
        return ABI_UI_OK;
    }
    if (strcmp(backend_id, "d3d12") == 0) {
        const KainComponentSurface* surface =
            kain_component_surface_resolve("d3d12");
        if (surface == NULL) return ABI_UI_INVALID_ARGUMENT;
        int64_t d3d12_session = surface->session_create(
            session->window_title, session->width, session->height);
        if (d3d12_session < 0) return ABI_UI_CAPACITY_EXCEEDED;
        session->host_backend[0] = '\0';
        snprintf(session->host_backend, sizeof(session->host_backend), "d3d12");
        session->component_surface = surface;
        session->component_session_id = d3d12_session;
        session->host_attached = 1;
        return ABI_UI_OK;
    }
    if (strcmp(backend_id, "webgpu") == 0) {
        const KainComponentSurface* surface =
            kain_component_surface_resolve("webgpu");
        if (surface == NULL) return ABI_UI_INVALID_ARGUMENT;
        int64_t webgpu_session = surface->session_create(
            session->window_title, session->width, session->height);
        if (webgpu_session < 0) return ABI_UI_CAPACITY_EXCEEDED;
        session->host_backend[0] = '\0';
        snprintf(session->host_backend, sizeof(session->host_backend), "webgpu");
        session->component_surface = surface;
        session->component_session_id = webgpu_session;
        session->host_attached = 1;
        return ABI_UI_OK;
    }
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
    if (session->component_surface != NULL) {
        session->component_surface->present(session->component_session_id);
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

// ── Framebuffer accessors for direct pixel rendering from Kain ──────
// These expose the DIB framebuffer to Kain code so it can write pixels
// directly, bypassing the node tree renderer and layout engine.

int64_t abi_ui_framebuffer_ptr(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session || !session->host_state) return 0;
    KainWin32UiHost* host = (KainWin32UiHost*)session->host_state;
    return (int64_t)(uintptr_t)host->framebuffer;
}

int64_t abi_ui_framebuffer_width(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session || !session->host_state) return 0;
    KainWin32UiHost* host = (KainWin32UiHost*)session->host_state;
    return host->width;
}

int64_t abi_ui_framebuffer_height(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session || !session->host_state) return 0;
    KainWin32UiHost* host = (KainWin32UiHost*)session->host_state;
    return host->height;
}

int64_t abi_ui_framebuffer_stride(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session || !session->host_state) return 0;
    KainWin32UiHost* host = (KainWin32UiHost*)session->host_state;
    return host->fb_stride / 4;  // stride in uint32_t elements
}

int64_t abi_ui_invalidate_window(int64_t session_id) {
    KainNativeUiSession* session = abi_ui_find_session(session_id);
    if (!session || !session->host_state) return -1;
    KainWin32UiHost* host = (KainWin32UiHost*)session->host_state;
    InvalidateRect(host->hwnd, NULL, FALSE);
    return 0;
}

void abi_ui_host_adapter_shutdown(KainNativeUiSession* session) {
    if (!session) {
        return;
    }
    if (session->component_surface != NULL && session->component_session_id > 0) {
        session->component_surface->session_destroy(session->component_session_id);
        session->component_surface = NULL;
        session->component_session_id = 0;
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
