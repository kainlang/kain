// ============================================================================
// direct_win32.c — Standalone Win32 window with colored UI (works)
// ============================================================================
// Uses the proven GDI + CreateDIBSection pattern from Kain's calculator.c.
// Creates a real window with colored rectangles + GDI text drawn directly
// into the framebuffer. No Kain dependency.
// ============================================================================
// Build:
//   set LIB=C:\...\MSVC\lib\x64;C:\...\Windows Kits\10\Lib\10.0.26100.0\um\x64
//   clang -std=c11 -g -O0 direct_win32.c -luser32 -lgdi32 -o direct_win32.exe
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ── Color palette (0xAABBGGRR as stored in framebuffer) ────────────────
// GDI 32-bit DIB uses 0xAABBGGRR byte order in memory (little-endian).
// A uint32_t value 0xFF21D4A1 = Blue=0xA1, Green=0xD4, Red=0x21, Alpha=0xFF
#define C_BG      0xFF0F172A  // deep navy
#define C_SURFACE 0xFF1E293B  // blue-gray
#define C_CARD    0xFF252540  // card surface
#define C_HEADER  0xFF1A1A2E  // header bar
#define C_SIDEBAR 0xFF16162A  // sidebar
#define C_ACCENT  0xFF21D4A1  // green accent
#define C_ACCENT2 0xFF4A90D9  // blue
#define C_ACCENT3 0xFFE8914A  // orange
#define C_ACCENT4 0xFFE84A5F  // red
#define C_TEXT    0xFFE8E8F0  // white text
#define C_TEXT_DIM 0xFF8888A0 // dim text

// ── App state ───────────────────────────────────────────────────────────
typedef struct {
    HWND hwnd;
    int width;
    int height;
    int running;
    uint32_t* framebuffer;
    int stride;         // in uint32_t elements (= width)
    HDC hdc_buffer;     // permanent DC with DIB selected
    HBITMAP hbitmap;
    int frame_count;
} AppState;

static AppState g_state = {0};

// ── Pixel helpers ───────────────────────────────────────────────────────
static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    uint32_t* fb = g_state.framebuffer;
    int stride = g_state.stride;
    int fb_w = g_state.width;
    int fb_h = g_state.height;
    for (int r = y; r < y + h && r < fb_h; r++) {
        if (r < 0) continue;
        for (int c = x; c < x + w && c < fb_w; c++) {
            if (c < 0) continue;
            fb[r * stride + c] = color;
        }
    }
}

// ── Paint the entire UI — this is the "Scene" composed directly ←─
// All positions are EXPLICIT — no layout engine involved.
static void paint_scene(void) {
    int w = g_state.width;
    int h = g_state.height;
    HDC dc = g_state.hdc_buffer;

    // 1. Clear to background
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++)
            g_state.framebuffer[r * g_state.stride + c] = C_BG;

    // 2. Header bar
    fill_rect(0, 0, w, 56, C_HEADER);
    fill_rect(0, 54, w, 2, C_ACCENT);  // accent line

    // GDI Text in header
    if (dc) {
        SetTextColor(dc, RGB(0xE8, 0xE8, 0xF0));
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        TextOutA(dc, 14, 16, "Kain Direct Win32", 17);
        SetTextColor(dc, RGB(0x88, 0x88, 0xA0));
        TextOutA(dc, 14, 34, "GDI DIB Backend  |  Z3-Verified  |  No Layout Engine", 50);
    }

    // Green status dot in header
    fill_rect(16, 18, 20, 20, C_ACCENT);
    fill_rect(18, 20, 16, 16, 0xFF2A2A4E);  // inner cutout

    // 3. Sidebar
    fill_rect(0, 56, 200, h - 84, C_SIDEBAR);
    fill_rect(199, 56, 1, h - 84, 0xFF3A3A5C);

    // Sidebar items
    const char* menu[] = {"Dashboard", "Analytics", "Explorer", "Settings"};
    uint32_t dots[] = {C_ACCENT, C_ACCENT2, C_ACCENT3, C_TEXT_DIM};
    for (int i = 0; i < 4; i++) {
        int iy = 66 + i * 44;
        uint32_t item_bg = (i == 0) ? 0xFF2A2A4E : C_SIDEBAR;
        fill_rect(12, iy, 176, 36, item_bg);
        fill_rect(20, iy + 12, 8, 8, dots[i]);
        if (dc) {
            SetTextColor(dc, (i == 0) ? RGB(232, 232, 240) : RGB(136, 136, 160));
            TextOutA(dc, 36, iy + 10, menu[i], (int)strlen(menu[i]));
        }
    }

    // 4. Card row
    int card_y = 66, card_w = 135, card_h = 80, gap = 8, start_x = 212;
    uint32_t card_colors[] = {C_ACCENT, C_ACCENT2, C_ACCENT3, C_ACCENT4};
    const char* card_labels[] = {"16", "85%", "4.2K", "12ms"};
    const char* card_titles[] = {"Sessions", "CPU", "Requests", "Latency"};

    for (int i = 0; i < 4; i++) {
        int cx = start_x + i * (card_w + gap);
        fill_rect(cx, card_y, card_w, card_h, card_colors[i]);
        fill_rect(cx + 1, card_y + 1, card_w - 2, card_h - 2, C_CARD);
        fill_rect(cx, card_y, card_w, 3, card_colors[i]);  // top stripe
        if (dc) {
            SetTextColor(dc, RGB(232, 232, 240));
            HFONT hf_val = CreateFontA(28, 0, 0, 0, FW_BOLD, 0, 0, 0,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                ANTIALIASED_QUALITY, DEFAULT_PITCH, "Segoe UI");
            HFONT old = (HFONT)SelectObject(dc, hf_val);
            TextOutA(dc, cx + 10, card_y + 8, card_labels[i], (int)strlen(card_labels[i]));
            SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
            DeleteObject(hf_val);
            SetTextColor(dc, RGB(136, 136, 160));
            TextOutA(dc, cx + 10, card_y + 54, card_titles[i], (int)strlen(card_titles[i]));
        }
    }

    // 5. Graph area
    fill_rect(start_x, 158, 580, 200, C_SURFACE);
    fill_rect(start_x, 158, 580, 1, 0xFF3A3A5C);

    // Animated graph bars
    for (int i = 0; i < 8; i++) {
        int raw = (g_state.frame_count * 4 + i * 37) % 160;
        int bh = 20 + raw;
        if (bh > 160) bh = 320 - bh;
        int bx = start_x + 12 + i * 72;
        int by = 158 + 200 - 8 - bh;
        fill_rect(bx, by, 60, bh, card_colors[i % 4]);
    }

    // Section label
    if (dc) {
        SetTextColor(dc, RGB(136, 136, 160));
        char section[64];
        snprintf(section, sizeof(section), "SYSTEM ACTIVITY  |  frame %d", g_state.frame_count);
        TextOutA(dc, start_x + 8, 370, section, (int)strlen(section));
    }

    // 6. Info panel
    fill_rect(start_x, 384, 580, 40, C_SURFACE);
    fill_rect(start_x, 384, 580, 1, 0xFF3A3A5C);
    if (dc) {
        SetTextColor(dc, RGB(136, 136, 160));
        TextOutA(dc, start_x + 12, 396,
                 "Win32  |  Direct DIB Framebuffer  |  GDI  |  Z3-Verified  |  No Layout Engine", 82);
    }

    // 7. Floating orbs (animated)
    int orb1_x = 220 + (g_state.frame_count * 2 % 450);
    int orb1_y = 445 + (int)(sin(g_state.frame_count * 0.05) * 20);
    fill_rect(orb1_x, orb1_y, 20, 20, C_ACCENT);
    fill_rect(orb1_x + 2, orb1_y + 2, 16, 16, C_BG);  // cutout
    fill_rect(orb1_x + 4, orb1_y + 4, 12, 12, C_ACCENT);  // inner

    int orb2_x = 600 - (g_state.frame_count * 2 % 350);
    int orb2_y = 445 + (int)(cos(g_state.frame_count * 0.07) * 15);
    fill_rect(orb2_x, orb2_y, 16, 16, C_ACCENT4);

    // 8. Status bar
    fill_rect(0, h - 28, w, 28, C_HEADER);
    fill_rect(12, h - 22, 12, 12, C_ACCENT);  // green status dot
    if (dc) {
        SetTextColor(dc, RGB(136, 136, 160));
        char status[128];
        snprintf(status, sizeof(status), "Running  |  %dx%d  |  Frame %d  |  Close to exit",
                 g_state.width, g_state.height, g_state.frame_count);
        TextOutA(dc, 30, h - 22, status, (int)strlen(status));
    }
}

// ── Window procedure ───────────────────────────────────────────────────
static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (hdc && g_state.hdc_buffer) {
                // BitBlt from the permanent DC (which has the DIB selected)
                BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
                       ps.rcPaint.right - ps.rcPaint.left,
                       ps.rcPaint.bottom - ps.rcPaint.top,
                       g_state.hdc_buffer, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CLOSE:
        case WM_DESTROY:
            g_state.running = 0;
            DestroyWindow(hwnd);
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

// ── Main ────────────────────────────────────────────────────────────────
int main(void) {
    int win_w = 800, win_h = 600;

    // Register window class
    WNDCLASSA wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "DirectWin32";
    RegisterClassA(&wc);

    // Create window
    g_state.hwnd = CreateWindowExA(0, "DirectWin32", "Direct Win32 — Colored UI Demo",
                                   WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                   100, 100, win_w, win_h, NULL, NULL,
                                   GetModuleHandleA(NULL), NULL);
    if (!g_state.hwnd) {
        printf("FAIL: CreateWindowEx\n");
        return 1;
    }

    // Get actual client rect (DPI scaling may differ from requested)
    RECT client;
    GetClientRect(g_state.hwnd, &client);
    g_state.width = client.right - client.left;
    g_state.height = client.bottom - client.top;
    if (g_state.width <= 0) g_state.width = win_w;
    if (g_state.height <= 0) g_state.height = win_h;

    // Create DIB framebuffer
    HDC hdc_screen = GetDC(NULL);
    g_state.hdc_buffer = CreateCompatibleDC(hdc_screen);
    if (g_state.hdc_buffer) {
        BITMAPINFO bmi = {0};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = g_state.width;
        bmi.bmiHeader.biHeight = -g_state.height;  // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        g_state.hbitmap = CreateDIBSection(hdc_screen, &bmi, DIB_RGB_COLORS,
                                           (void**)&g_state.framebuffer, NULL, 0);
        if (g_state.hbitmap) {
            SelectObject(g_state.hdc_buffer, g_state.hbitmap);
            g_state.stride = g_state.width;  // in uint32_t (= DIB width)
        }
    }
    ReleaseDC(NULL, hdc_screen);

    if (!g_state.framebuffer) {
        printf("FAIL: CreateDIBSection\n");
        return 1;
    }

    g_state.running = 1;
    printf("✅ Window created: %dx%d framebuffer=%p\n", g_state.width, g_state.height,
           (void*)g_state.framebuffer);
    printf("Rendering colored UI for 30 seconds. Close the window to exit.\n");

    // ── Frame loop ────────────────────────────────────────────────
    DWORD start = GetTickCount();
    while (g_state.running && (GetTickCount() - start) < 30000) {
        // Message pump
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_state.running = 0;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        // Paint scene directly into framebuffer
        paint_scene();

        // Trigger WM_PAINT to blit framebuffer to screen
        InvalidateRect(g_state.hwnd, NULL, FALSE);

        // Frame counter + status
        g_state.frame_count++;
        Sleep(33);  // ~30 FPS
    }

    printf("Done after %d frames.\n", g_state.frame_count);
    return 0;
}
