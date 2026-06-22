// kauri_host.c — Frameless host window with embedded browser (Win32)
// Uses the Windows WebBrowser ActiveX control — no extra SDK needed.
// Include from Kain:  include "kauri_host.h" as host

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <initguid.h>
#include <exdisp.h>       // IWebBrowser2
#include <shlobj.h>
#include <shlwapi.h>

// ── Globals ──
static HWND g_hwnd = NULL;
static HWND g_browser_hwnd = NULL;
static IWebBrowser2* g_browser = NULL;
static int g_running = 0;

// ── Forward declarations ──
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l);

// ── Register browser CLSID ──
// {8856F961-340A-11D0-A96B-00C04FD705A2} — Shell.Explorer / WebBrowser
DEFINE_GUID(CLSID_WebBrowser, 0x8856F961, 0x340A, 0x11D0, 0xA9, 0x6B, 0x00, 0xC0, 0x4F, 0xD7, 0x05, 0xA2);
DEFINE_GUID(IID_IWebBrowser2, 0xD30C1661, 0xCDAF, 0x11D0, 0x8A, 0x3E, 0x00, 0xC0, 0x4F, 0xC9, 0xE2, 0x6E);

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
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
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

    // Create WebBrowser ActiveX control as a child of our window
    g_browser_hwnd = CreateWindowExA(0, "Shell.Explorer", NULL,
        WS_CHILD | WS_VISUS | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, width, height,
        g_hwnd, NULL, inst, NULL);

    if (!g_browser_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
        OleUninitialize();
        return -3;
    }

    // Get IWebBrowser2 interface from the control
    IOleObject* ole_obj = NULL;
    IOleClientSite* site = NULL; // minimal site (we don't need full container)

    // Query for IOleObject
    IUnknown* unk = NULL;
    SendMessage(g_browser_hwnd, WM_HTML_GETOBJECT, 0, (LPARAM)&unk);
    if (unk) {
        unk->QueryInterface(IID_IWebBrowser2, (void**)&g_browser);
        unk->Release();
    } else {
        // Fallback: try direct COM creation
        hr = CoCreateInstance(&CLSID_WebBrowser, NULL, CLSCTX_INPROC_SERVER,
                              &IID_IWebBrowser2, (void**)&g_browser);
        if (SUCCEEDED(hr) && g_browser) {
            // Set the parent window
            IOleObject* ole = NULL;
            g_browser->QueryInterface(IID_IOleObject, (void**)&ole);
            if (ole) {
                ole->SetClientSite(NULL); // minimal
                ole->Release();
            }

            // Resize
            IUnknown* ctrl = NULL;
            g_browser->QueryInterface(IID_IUnknown, (void**)&ctrl);
            if (ctrl) {
                IOleInPlaceObject* ipo = NULL;
                ctrl->QueryInterface(IID_IOleInPlaceObject, (void**)&ipo);
                if (ipo) {
                    RECT rc = {0, 0, width, height};
                    ipo->SetObjectRects(&rc, &rc);
                    ipo->Release();
                }
                ctrl->Release();
            }
        }
    }

    if (!g_browser) {
        DestroyWindow(g_browser_hwnd);
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
        OleUninitialize();
        return -4;
    }

    // Navigate to the URL
    if (g_browser) {
        VARIANT v_url;
        VariantInit(&v_url);
        v_url.vt = VT_BSTR;
        int url_len = MultiByteToWideChar(CP_UTF8, 0, url, -1, NULL, 0);
        v_url.bstrVal = SysAllocStringLen(NULL, url_len - 1);
        MultiByteToWideChar(CP_UTF8, 0, url, -1, v_url.bstrVal, url_len);

        VARIANT v_empty;
        VariantInit(&v_empty);
        v_empty.vt = VT_EMPTY;

        g_browser->Navigate2(&v_url, &v_empty, &v_empty, &v_empty, &v_empty);
        SysFreeString(v_url.bstrVal);
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
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            g_running = 0;
            return 1;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
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
    if (g_hwnd) PostMessage(g_hwnd, WM_CLOSE, 0, 0);
}

// ── Window procedure ──
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(hwnd, msg, w, l);
            if (hit == HTCLIENT) return HTCAPTION; // drag from anywhere
            return hit;
        }
        case WM_SIZE: {
            if (g_browser_hwnd) {
                RECT r;
                GetClientRect(hwnd, &r);
                SetWindowPos(g_browser_hwnd, NULL, 0, 0,
                             r.right - r.left, r.bottom - r.top,
                             SWP_NOZORDER);
            }
            return 0;
        }
        case WM_CLOSE:
            g_running = 0;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (g_browser) { g_browser->Release(); g_browser = NULL; }
            g_browser_hwnd = NULL;
            g_hwnd = NULL;
            PostQuitMessage(0);
            OleUninitialize();
            return 0;
        default:
            return DefWindowProc(hwnd, msg, w, l);
    }
}
