// ============================================================================
//  Path C: Pure GDI — Bypass Kain entirely
//  ============================================================================
//  Control test: creates a raw Win32 window with its own DIB framebuffer and
//  renders a gradient + shapes directly into it. This proves the compilation
//  environment works and the GPU/driver can produce visible content.
//  ============================================================================
//
//  Compile:
//    clang -std=c11 -g -O0 path_c_pure_gdi.c ^
//      -luser32 -lgdi32 -o path_c_pure_gdi.exe
//
//  Or via build_c.bat

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

// ── Window state ────────────────────────────────────────────────────────
typedef struct {
    HWND hwnd;
    int width;
    int height;
    int running;
    uint32_t* framebuffer;
    int fb_stride;  // in uint32_t elements
    HDC hdc_mem;    // memory DC with DIB section permanently selected
    HBITMAP hbmp;
} AppState;

static AppState g_state = {0};

// ── Pixel helpers ───────────────────────────────────────────────────────
static uint32_t rgba(int r, int g, int b, int a) {
    return ((uint32_t)(a & 0xFF) << 24) |
           ((uint32_t)(b & 0xFF) << 16) |
           ((uint32_t)(g & 0xFF) <<  8) |
           ((uint32_t)(r & 0xFF));
}

static void clear_framebuffer(uint32_t color) {
    AppState* s = &g_state;
    int total = s->width * s->height;
    for (int i = 0; i < total; i++) {
        s->framebuffer[i] = color;
    }
}

static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    AppState* s = &g_state;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > s->width)  w = s->width - x;
    if (y + h > s->height) h = s->height - y;
    if (w <= 0 || h <= 0) return;

    for (int row = y; row < y + h; row++) {
        uint32_t* dst = s->framebuffer + row * s->fb_stride + x;
        for (int col = 0; col < w; col++) {
            dst[col] = color;
        }
    }
}

// ── Render a frame (gradient + shapes) ─────────────────────────────────
static void render_frame(void) {
    AppState* s = &g_state;
    int w = s->width, h = s->height;

    // Dark background
    clear_framebuffer(rgba(26, 26, 36, 255));

    // ── Colorful gradient bars ────────────────────────────────────
    for (int row = 0; row < h; row++) {
        uint32_t* pix = s->framebuffer + row * s->fb_stride;
        for (int col = 0; col < w; col++) {
            uint8_t r = (uint8_t)((col * 255) / w);
            uint8_t g = (uint8_t)((row * 255) / h);
            uint8_t b = (uint8_t)(((w - col) * 255) / w);
            if (row < 60 || row >= h - 32) {
                // Header/status bar: dark
                pix[col] = rgba(30, 30, 50, 255);
            } else if (col < 220) {
                // Sidebar: dark blue
                pix[col] = rgba(32, 32, 56, 255);
            } else {
                // Gradient background
                pix[col] = rgba(r, g, b, 255);
            }
        }
    }

    // ── Status cards (4 colorful rectangles) ──────────────────────
    int card_w = (w - 260) / 4;
    int card_h = 100;
    int card_y = 70;
    uint32_t card_colors[] = {
        rgba(33, 212, 161, 200),  // green
        rgba(74, 144, 217, 200),  // blue
        rgba(232, 145, 74, 200),  // orange
        rgba(232, 74, 95, 200),   // red
    };
    for (int i = 0; i < 4; i++) {
        int cx = 236 + i * (card_w + 8);
        fill_rect(cx, card_y, card_w, card_h, rgba(37, 37, 64, 255));
        fill_rect(cx, card_y + 2, card_w, 3, card_colors[i]);
        // Inner value rect
        fill_rect(cx + 12, card_y + 14, card_w - 24, 32, rgba(46, 46, 72, 255));
    }

    // ── Graph area (simulated chart bars) ─────────────────────────
    int graph_x = 236, graph_y = 190;
    int graph_w = w - 244, graph_h = 180;
    fill_rect(graph_x, graph_y, graph_w, graph_h, rgba(37, 37, 64, 255));
    fill_rect(graph_x, graph_y, graph_w, 1, rgba(58, 58, 92, 255));

    int bar_count = 8;
    int bar_w = (graph_w - 24 - (bar_count - 1) * 4) / bar_count;
    if (bar_w < 4) bar_w = 4;
    for (int i = 0; i < bar_count; i++) {
        int bh = 20 + (i * 17 + 7) % (graph_h - 40);
        int bx = graph_x + 12 + i * (bar_w + 4);
        int by = graph_y + graph_h - 8 - bh;
        fill_rect(bx, by, bar_w, bh, card_colors[i % 4]);
    }

    // ── Accent line ───────────────────────────────────────────────
    fill_rect(0, 58, w, 2, rgba(33, 212, 161, 255));
}

// ── Window procedure ───────────────────────────────────────────────────
static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    AppState* s = &g_state;

    switch (msg) {
        case WM_CLOSE:
            s->running = 0;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            s->running = 0;
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;  // We paint everything
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (hdc && s->hdc_mem) {
                BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
                       ps.rcPaint.right - ps.rcPaint.left,
                       ps.rcPaint.bottom - ps.rcPaint.top,
                       s->hdc_mem, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

// ── Main ────────────────────────────────────────────────────────────────
int main(void) {
    AppState* s = &g_state;
    s->width = 1280;
    s->height = 720;
    s->running = 1;
    s->fb_stride = s->width;  // in uint32_t elements

    HINSTANCE hinst = GetModuleHandleA(NULL);

    // Register window class
    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "PathCGDI";
    RegisterClassA(&wc);

    // Create DIB section + memory DC
    HDC hdc_screen = GetDC(NULL);
    s->hdc_mem = CreateCompatibleDC(hdc_screen);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = s->width;
    bmi.bmiHeader.biHeight = -s->height;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    s->hbmp = CreateDIBSection(hdc_screen, &bmi, DIB_RGB_COLORS,
                               (void**)&s->framebuffer, NULL, 0);
    if (s->hbmp && s->hdc_mem) {
        SelectObject(s->hdc_mem, s->hbmp);
    }
    ReleaseDC(NULL, hdc_screen);

    if (!s->framebuffer) {
        fprintf(stderr, "FAIL: CreateDIBSection returned NULL\n");
        return 1;
    }
    printf("DIB framebuffer created: %dx%d, ptr=%p\n", s->width, s->height, (void*)s->framebuffer);

    // Create window
    s->hwnd = CreateWindowExA(0, "PathCGDI", "Path C: Pure GDI Control Test",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, s->width, s->height,
        NULL, NULL, hinst, NULL);
    if (!s->hwnd) {
        fprintf(stderr, "FAIL: CreateWindowEx\n");
        return 1;
    }

    printf("Window created. HWND=%p\n", (void*)s->hwnd);

    // Render first frame immediately
    render_frame();
    InvalidateRect(s->hwnd, NULL, FALSE);

    printf("Entering message loop. Close the window to exit.\n");
    printf("============================================================\n");

    // Message loop
    MSG msg;
    int frame = 0;
    while (s->running) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) s->running = 0;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!s->running) break;

        // Re-render with animation every ~60 frames
        frame++;
        if (frame % 60 == 0) {
            // Animate: shift colors
            render_frame();
            InvalidateRect(s->hwnd, NULL, FALSE);
        }

        Sleep(16);
    }

    // Cleanup
    if (s->hbmp) DeleteObject(s->hbmp);
    if (s->hdc_mem) DeleteDC(s->hdc_mem);
    if (s->hwnd) DestroyWindow(s->hwnd);

    printf("Done. %d frames.\n", frame);
    return 0;
}
