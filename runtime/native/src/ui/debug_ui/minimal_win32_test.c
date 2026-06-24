// ============================================================================
//  Minimal Win32 DIB Test — just get SOMETHING visible on screen
//  ============================================================================
//  No Kain UI system. Just:
//    1. Create a window with explicit coordinates
//    2. Create a DIB section 
//    3. Draw colored bars into the DIB
//    4. BitBlt to screen in a loop
//
//  Compile:
//    clang -std=c11 -Wall -Wextra -g -O0 minimal_win32_test.c -luser32 -lgdi32 -o minimal_win32_test.exe
// ============================================================================

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    HWND hwnd;
    int client_w;
    int client_h;
    uint32_t* pixels;
    int stride;
    HDC mem_dc;
    HBITMAP hbitmap;
    int running;
} AppState;

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    AppState* s = (AppState*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTA* cs = (CREATESTRUCTA*)l;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcA(hwnd, msg, w, l);
    }
    case WM_CLOSE:
        if (s) s->running = 0;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (s) s->running = 0;
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return 1;  // We paint everything
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (hdc && s && s->pixels) {
            HDC mem = CreateCompatibleDC(hdc);
            if (mem) {
                HBITMAP old = (HBITMAP)SelectObject(mem, s->hbitmap);
                BitBlt(hdc,
                       ps.rcPaint.left, ps.rcPaint.top,
                       ps.rcPaint.right - ps.rcPaint.left,
                       ps.rcPaint.bottom - ps.rcPaint.top,
                       mem,
                       ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
                SelectObject(mem, old);
                DeleteDC(mem);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE: {
        if (s) {
            int cw = LOWORD(l), ch = HIWORD(l);
            if (cw > 0 && ch > 0) {
                s->client_w = cw;
                s->client_h = ch;
            }
        }
        return 0;
    }
    }
    return DefWindowProcA(hwnd, msg, w, l);
}

int main(void) {
    AppState state;
    memset(&state, 0, sizeof(state));
    state.running = 1;

    // Register
    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "MinimalTest";
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        fprintf(stderr, "FAIL: RegisterClassA\n");
        return 1;
    }

    // Create window — explicit size + position
    int desired_cx = 800;
    int desired_cy = 600;
    
    RECT wr = {0, 0, desired_cx, desired_cy};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    int win_w = wr.right - wr.left;
    int win_h = wr.bottom - wr.top;

    printf("Desired client: %dx%d\n", desired_cx, desired_cy);
    printf("Window size: %dx%d\n", win_w, win_h);

    // Use explicit position (100,100) — never off-screen
    state.hwnd = CreateWindowExA(
        0, "MinimalTest", "Minimal Win32 Test",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, win_w, win_h,
        NULL, NULL, GetModuleHandleA(NULL), &state);
    if (!state.hwnd) {
        fprintf(stderr, "FAIL: CreateWindowExA (err=%lu)\n", GetLastError());
        return 1;
    }

    // After creation, check actual client size
    RECT client_rect;
    GetClientRect(state.hwnd, &client_rect);
    printf("Actual client rect: %d x %d\n",
           client_rect.right, client_rect.bottom);

    // Force update WM_SIZE values
    state.client_w = client_rect.right;
    state.client_h = client_rect.bottom;

    // Create DIB with ACTUAL client size
    HDC screen_dc = GetDC(NULL);
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = state.client_w;
    bmi.bmiHeader.biHeight = -state.client_h;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    state.hbitmap = CreateDIBSection(screen_dc, &bmi, DIB_RGB_COLORS,
                                      (void**)&state.pixels, NULL, 0);
    if (!state.hbitmap || !state.pixels) {
        fprintf(stderr, "FAIL: CreateDIBSection (err=%lu)\n", GetLastError());
        ReleaseDC(NULL, screen_dc);
        return 1;
    }

    state.mem_dc = CreateCompatibleDC(screen_dc);
    SelectObject(state.mem_dc, state.hbitmap);
    state.stride = state.client_w;  // 32-bit pixels
    ReleaseDC(NULL, screen_dc);

    printf("DIB created: %dx%d, pixels=%p\n",
           state.client_w, state.client_h, (void*)state.pixels);

    // Fill DIB with initial test pattern — dark indigo background
    uint32_t bg = 0xFF1A1A24;
    for (int y = 0; y < state.client_h; y++)
        for (int x = 0; x < state.client_w; x++)
            state.pixels[y * state.stride + x] = bg;

    // Draw some colored rectangles
    uint32_t colors[] = {0xFFE84A5F, 0xFF21D4A1, 0xFF4A90D9, 0xFFE8914A, 0xFF8888A0};
    for (int i = 0; i < 5; i++) {
        int x = 30 + i * 160;
        int y = 50;
        int w = 140;
        int h = 120;
        for (int row = y; row < y + h && row < state.client_h; row++)
            for (int col = x; col < x + w && col < state.client_w; col++)
                state.pixels[row * state.stride + col] = colors[i];
    }

    // Draw some text via GDI to prove the DC works
    HDC text_dc = state.mem_dc;
    SetBkMode(text_dc, TRANSPARENT);
    SetTextColor(text_dc, RGB(255, 255, 255));
    TextOutA(text_dc, 30, 20, "Kain UI - Minimal Test", 22);
    TextOutA(text_dc, 30, 200, "Red Green Blue Orange Gray", 26);
    TextOutA(text_dc, 30, 230, "If you can read this, the DIB pipeline works!", 49);

    // Force initial paint
    InvalidateRect(state.hwnd, NULL, FALSE);
    UpdateWindow(state.hwnd);

    printf("Window should now show content. Running message loop...\n");
    printf("Close window to exit.\n");

    // Message loop
    while (state.running) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) state.running = 0;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        // No Sleep — use msg wait instead
        if (state.running) {
            WaitMessage();
        }
    }

    // Cleanup
    if (state.hbitmap) DeleteObject(state.hbitmap);
    if (state.mem_dc) DeleteDC(state.mem_dc);
    if (state.hwnd && IsWindow(state.hwnd)) DestroyWindow(state.hwnd);
    printf("Done.\n");
    return 0;
}
