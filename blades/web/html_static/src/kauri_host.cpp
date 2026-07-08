// kauri_host.c — Frameless host window (Win32)
// Include from Kain:  include "kauri_host.h" as host
// Discovered automatically beside this file.
//
// Creates a borderless Win32 window and embeds the WebBrowser control
// (Trident/EdgeHTML engine) pointing at the Kain HTTP server.
// Uses raw COM — no C++ headers needed.

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMSG
#define NOBITMAP
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOSYSCOMMANDS
#define NOSHOWWINDOW
#undef NOUSER
#undef NOMSG

#include <windows.h>
#include <stdint.h>

// ── COM declarations (minimal, avoid exdisp.h C++ dependency) ──
// CLSID_WebBrowser: {8856F961-340A-11D0-A96B-00C04FD705A2}
// IID_IWebBrowser2: {D30C1661-CDAF-11D0-8A3E-00C04FC9E26E}
// We use Shell.Explorer window class + direct COM via IOleObject/IPersistStreamInit

#include <initguid.h>
DEFINE_GUID(CLSID_WebBrowser, 0x8856F961, 0x340A, 0x11D0, 0xA9, 0x6B, 0x00, 0xC0, 0x4F, 0xD7, 0x05, 0xA2);

// ── Forward declarations ──
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l);

// ── Globals ──
static HWND g_hwnd = NULL;
static HWND g_browser = NULL;
static int g_running = 0;
static IOleObject* g_ole = NULL;
static IWebBrowser2* g_web = NULL;

// IWebBrowser2 interface (minimal — just what we need for Navigate2)
// Full IWebBrowser2 has ~100 methods; we declare only Navigate2.
// The vtable offset for Navigate2 is method 12 (0-based).
// Instead of a full interface declaration, we use direct vtable dispatch.

// Actually, simplest reliable approach: use the Shell.Explorer HWND
// and send it navigation commands via the IOleObject/IPersistStreamInit path.

// ── Init: create frameless window with embedded browser ──
int kauri_host_init(const char* url, int32_t width, int32_t height) {
    if (width <= 0) width = 900;
    if (height <= 0) height = 700;

    // Init COM
    HRESULT hr = OleInitialize(NULL);
    if (FAILED(hr)) return -1;

    // Register window class
    HINSTANCE inst = GetModuleHandleA(NULL);
    const char* CLASS = "KauriHost";
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS;
    RegisterClassA(&wc);

    // Create frameless window
    g_hwnd = CreateWindowExA(0, CLASS, "Kauri", WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, inst, NULL);
    if (!g_hwnd) { OleUninitialize(); return -2; }

    // Center on screen
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(g_hwnd, NULL, (sw - width) / 2, (sh - height) / 2,
                 width, height, SWP_NOZORDER);

    // Create WebBrowser ActiveX control as child window
    // Using ATL::CAxWindow equivalent via CreateWindow with Shell.Explorer
    g_browser = CreateWindowExA(0, "Shell.Explorer", NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, width, height,
        g_hwnd, NULL, inst, NULL);

    if (!g_browser) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
        OleUninitialize();
        return -3;
    }

    // Navigate: PostMessage with WM_COMMAND approach
    // Shell.Explorer handles URL navigation via a registered message
    // or we can use the IOleObject/IPersistMoniker path.
    // For simplicity, navigate by sending a URL via the browser's
    // IWebBrowser2 COM interface.

    // Get IWebBrowser2 from the control
    IUnknown* unk = NULL;
    SendMessage(g_browser, WM_HTML_GETOBJECT, 0, (LPARAM)&unk);

    if (unk) {
        // Use IWebBrowser2::Navigate2 via vtable directly
        // This is fragile but avoids needing exdisp.h
        // Navigate2 vtable offset for IWebBrowser2 is index 12
        void** vtable = *(void***)unk;
        if (vtable) {
            typedef HRESULT (__stdcall *Navigate2Fn)(IUnknown*, VARIANT*, VARIANT*, VARIANT*, VARIANT*);
            Navigate2Fn navigate2 = (Navigate2Fn)vtable[12];

            VARIANT v_url;
            VariantInit(&v_url);
            v_url.vt = VT_BSTR;
            int url_len = MultiByteToWideChar(CP_UTF8, 0, url, -1, NULL, 0);
            v_url.bstrVal = SysAllocStringLen(NULL, url_len - 1);
            if (v_url.bstrVal) {
                MultiByteToWideChar(CP_UTF8, 0, url, -1, v_url.bstrVal, url_len);

                VARIANT v_empty;
                VariantInit(&v_empty);
                v_empty.vt = VT_EMPTY;

                navigate2(unk, &v_url, &v_empty, &v_empty, &v_empty);
                SysFreeString(v_url.bstrVal);
            }
        }
        unk->lpVtbl->Release(unk);
    }

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    g_running = 1;
    return 0;
}

// ── Non-blocking message pump ──
int kauri_host_poll(void) {
    if (!g_running) return 1;
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { g_running = 0; return 1; }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}

// ── Set title ──
void kauri_host_set_title(const char* title) {
    if (g_hwnd) SetWindowTextA(g_hwnd, title);
}

// ── Shutdown ──
void kauri_host_shutdown(void) {
    g_running = 0;
    if (g_hwnd) PostMessageA(g_hwnd, WM_CLOSE, 0, 0);
}

// ── Window procedure ──
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProcA(hwnd, msg, w, l);
            if (hit == HTCLIENT) return HTCAPTION; // drag from anywhere
            return hit;
        }
        case WM_SIZE: {
            if (g_browser) {
                RECT r;
                GetClientRect(hwnd, &r);
                SetWindowPos(g_browser, NULL, 0, 0,
                    r.right - r.left, r.bottom - r.top, SWP_NOZORDER);
            }
            return 0;
        }
        case WM_CLOSE:
            g_running = 0;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (g_ole) g_ole->lpVtbl->Release(g_ole);
            g_ole = NULL;
            g_browser = NULL;
            g_hwnd = NULL;
            PostQuitMessage(0);
            OleUninitialize();
            return 0;
        default:
            return DefWindowProcA(hwnd, msg, w, l);
    }
}
