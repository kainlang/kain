#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static const char WINDOW_CLASS_NAME[] = "KainWindowClass";

static LRESULT CALLBACK win32_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProcA(hwnd, msg, wparam, lparam);
    }
}

void* win32_window_create(const char* title, int width, int height) {
    HINSTANCE instance = GetModuleHandleA(NULL);

    WNDCLASSEXA wc = {0};
    wc.cbSize        = sizeof(WNDCLASSEXA);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = win32_window_proc;
    wc.hInstance     = instance;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    ATOM atom = RegisterClassExA(&wc);
    if (atom == 0) {
        return NULL;
    }

    HWND hwnd = CreateWindowExA(
        0,
        WINDOW_CLASS_NAME,
        title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        NULL,
        NULL,
        instance,
        NULL
    );

    return (void*)hwnd;
}

void win32_window_show(void* hwnd) {
    ShowWindow((HWND)hwnd, SW_SHOWNORMAL);
    UpdateWindow((HWND)hwnd);
}

int win32_window_message_loop(void) {
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}

void win32_window_destroy(void* hwnd) {
    DestroyWindow((HWND)hwnd);
}

int win32_message_box(const char* text, const char* caption) {
    return MessageBoxA(NULL, text, caption, MB_OK);
}
