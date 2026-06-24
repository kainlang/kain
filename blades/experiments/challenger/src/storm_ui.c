// ============================================================================
//  storm_ui.c — Win32 + GDI + pixel rendering for Psychic Resonance Storm
//  Auto-discovered companion for storm_ui.h.
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
//  Constants
// ---------------------------------------------------------------------------

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600

#define BAR_WIDTH     300
#define BAR_HEIGHT    16

// ---------------------------------------------------------------------------
//  Global state
// ---------------------------------------------------------------------------

static HWND   g_hwnd          = NULL;
static HDC    g_hdc           = NULL;
static HDC    g_backbuffer_dc = NULL;
static HBITMAP g_backbuffer   = NULL;
static void*  g_bits          = NULL;
static int    g_running       = 1;
static int    g_display_w     = WINDOW_WIDTH;
static int    g_display_h     = WINDOW_HEIGHT;

// ---------------------------------------------------------------------------
//  Window procedure
// ---------------------------------------------------------------------------

static LRESULT CALLBACK storm_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            g_running = 0;
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            g_display_w = LOWORD(lParam);
            g_display_h = HIWORD(lParam);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

// ---------------------------------------------------------------------------
//  storm_init — Create window + backbuffer
// ---------------------------------------------------------------------------

int storm_init(void) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSEX wc = {0};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = storm_wndproc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "StormWindowClass";

    if (!RegisterClassEx(&wc)) return -1;

    RECT wr = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowEx(0, "StormWindowClass",
        "PSYCHIC RESONANCE STORM", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInstance, NULL);
    if (!g_hwnd) return -2;

    g_hdc = GetDC(g_hwnd);
    g_backbuffer_dc = CreateCompatibleDC(g_hdc);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = WINDOW_WIDTH;
    bmi.bmiHeader.biHeight      = -WINDOW_HEIGHT;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    g_backbuffer = CreateDIBSection(g_backbuffer_dc, &bmi, DIB_RGB_COLORS,
                                     &g_bits, NULL, 0);
    if (!g_backbuffer) return -3;
    SelectObject(g_backbuffer_dc, g_backbuffer);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    return 0;
}

// ---------------------------------------------------------------------------
//  storm_poll — Process messages. Returns 1 if window is closed.
// ---------------------------------------------------------------------------

int storm_poll(void) {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_QUIT) g_running = 0;
    }
    return g_running ? 0 : 1;
}

// ---------------------------------------------------------------------------
//  Pixel setter
// ---------------------------------------------------------------------------

static void set_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b) {
    if (x < 0 || x >= WINDOW_WIDTH || y < 0 || y >= WINDOW_HEIGHT) return;
    unsigned int* pixel = (unsigned int*)g_bits + (y * WINDOW_WIDTH + x);
    *pixel = (0xFF << 24) | (r << 16) | (g << 8) | b;
}

// ---------------------------------------------------------------------------
//  Draw bar chart
// ---------------------------------------------------------------------------

static void draw_bar(const char* label, int value, int max_val, int x, int y,
                     unsigned char r, unsigned char g, unsigned char b) {
    int filled = (value * BAR_WIDTH) / (max_val > 0 ? max_val : 1);
    if (filled > BAR_WIDTH) filled = BAR_WIDTH;
    if (filled < 0) filled = 0;

    SelectObject(g_backbuffer_dc, GetStockObject(DEFAULT_GUI_FONT));
    SetBkColor(g_backbuffer_dc, RGB(10, 10, 16));
    SetTextColor(g_backbuffer_dc, RGB(200, 200, 220));
    TextOutA(g_backbuffer_dc, x, y, label, (int)strlen(label));

    int bx, by;
    for (by = 0; by < BAR_HEIGHT; by++) {
        for (bx = 0; bx < BAR_WIDTH; bx++) {
            int px = x + 180 + bx;
            int py = y + by;
            if (bx < filled) {
                unsigned char mr = (unsigned char)((r * (BAR_HEIGHT - by) + 40 * by) / BAR_HEIGHT);
                unsigned char mg = (unsigned char)((g * (BAR_HEIGHT - by) + 20 * by) / BAR_HEIGHT);
                unsigned char mb = (unsigned char)((b * (BAR_HEIGHT - by) + 30 * by) / BAR_HEIGHT);
                set_pixel(px, py, mr, mg, mb);
            } else {
                unsigned char dim = (unsigned char)(20 + by * 2);
                set_pixel(px, py, dim, dim, dim + 5);
            }
        }
    }

    char val_str[32];
    snprintf(val_str, sizeof(val_str), "%d/%d", value, max_val);
    SetTextColor(g_backbuffer_dc, RGB(255, 255, 255));
    TextOutA(g_backbuffer_dc, x + 180 + BAR_WIDTH + 6, y, val_str, (int)strlen(val_str));
}

// ---------------------------------------------------------------------------
//  Draw storm border (lightning effect)
// ---------------------------------------------------------------------------

static void draw_border(int chaos) {
    int i, zig_count = (chaos * 12) / 100 + 3;
    int y = 0, x = 0;
    for (i = 0; i < zig_count && x < WINDOW_WIDTH; i++) {
        int j, seg = 15 + (chaos * 3);
        for (j = 0; j < seg && x < WINDOW_WIDTH; j++) {
            int bright = 180 + (chaos * 75) / 100;
            if (bright > 255) bright = 255;
            // Top edge
            set_pixel(x, y, (unsigned char)bright, (unsigned char)(bright/3), (unsigned char)(bright/8));
            // Bottom edge
            set_pixel(x, WINDOW_HEIGHT - 1 - y,
                (unsigned char)(bright/8), (unsigned char)(bright/3), (unsigned char)bright);
            x++;
        }
        y = (y == 0) ? 3 : 0;
    }
}

// ---------------------------------------------------------------------------
//  Draw wisp particles
// ---------------------------------------------------------------------------

static void draw_wisps(int count, int energy, int phase) {
    int i;
    for (i = 0; i < count && i < 64; i++) {
        int seed = i * 137 + phase * 53 + energy;
        int px = 350 + ((seed * 7) % 400);
        int py = 420 + ((seed * 13) % 140);
        unsigned char r = (unsigned char)((55 + energy * 2) % 256);
        unsigned char g = (unsigned char)((20 + i * 30) % 256);
        unsigned char b = (unsigned char)((100 + phase * 3) % 256);
        int dx, dy;
        for (dy = -2; dy <= 2; dy++)
            for (dx = -2; dx <= 2; dx++) {
                int dist = dx*dx + dy*dy;
                if (dist < 6)
                    set_pixel(px + dx, py + dy, r, g, b);
            }
    }
}

// ---------------------------------------------------------------------------
//  storm_frame — Render one frame
// ---------------------------------------------------------------------------

void storm_frame(
    int intensity, int turbulence, int chaos_seed,
    int wisp_count, int wisp_energy, int phase,
    int cascade_depth, int cycle,
    int pulse_fires, int resonate_fires
) {
    // Clear
    RECT clear = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    HBRUSH bg = CreateSolidBrush(RGB(8, 8, 14));
    FillRect(g_backbuffer_dc, &clear, bg);
    DeleteObject(bg);

    // Border
    draw_border(chaos_seed);

    // Title
    const char* mood;
    if (intensity < 10) mood = "CALM";
    else if (intensity < 25) mood = "BREEZE";
    else if (intensity < 50) mood = "STORM";
    else if (intensity < 75) mood = "TEMPEST";
    else mood = "PSYCHIC MAELSTROM";

    char title[64];
    snprintf(title, sizeof(title), "=== %s ===", mood);
    SetTextColor(g_backbuffer_dc, RGB(180, 80, 255));
    TextOutA(g_backbuffer_dc, 20, 15, title, (int)strlen(title));

    // Bars
    draw_bar("INTENSITY:",   intensity,  100, 20, 45,  255, 80,  80);
    draw_bar("CHAOS:",       chaos_seed % 100, 100, 20, 68,  200, 100, 255);
    draw_bar("TURBULENCE:",  turbulence, 64,  20, 91,  100, 200, 80);
    draw_bar("PHASE:",       phase,      360, 20, 114, 80,  200, 255);

    // Cascade depth
    int i;
    for (i = 0; i < cascade_depth && i < 8; i++) {
        int cy = 150 + i * 18;
        int cw = 20 + (cascade_depth - i) * 35;
        unsigned char cr = (unsigned char)(80 + i * 20);
        unsigned char cg = (unsigned char)(30 + i * 12);
        unsigned char cb = (unsigned char)(220 - i * 18);
        int bx, by;
        for (by = 0; by < 14; by++)
            for (bx = 0; bx < cw; bx++)
                set_pixel(20 + bx, cy + by, cr, cg, cb);
    }

    char depth_str[64];
    snprintf(depth_str, sizeof(depth_str), "CASCADE DEPTH: %d", cascade_depth);
    SetTextColor(g_backbuffer_dc, RGB(140, 140, 200));
    TextOutA(g_backbuffer_dc, 20, 150 + cascade_depth * 18 + 4, depth_str, (int)strlen(depth_str));

    // Wisp particles
    draw_wisps(wisp_count, wisp_energy, phase);

    // Info text
    char info[80];
    snprintf(info, sizeof(info), "WISPS: %d  ENERGY: %d  CYCLE: %d  PHASE: %d",
             wisp_count, wisp_energy, cycle, phase);
    SetTextColor(g_backbuffer_dc, RGB(80, 220, 140));
    TextOutA(g_backbuffer_dc, 20, 315, info, (int)strlen(info));

    // Telemetry
    char tele[80];
    snprintf(tele, sizeof(tele), "PULSE: %d  RESONATE: %d", pulse_fires, resonate_fires);
    SetTextColor(g_backbuffer_dc, RGB(200, 200, 80));
    TextOutA(g_backbuffer_dc, 20, 340, tele, (int)strlen(tele));

    char footer[120];
    snprintf(footer, sizeof(footer),
        "[pulse 8ms | resonate cascade 6-layer | actor x8 | feedback loop active]");
    SetTextColor(g_backbuffer_dc, RGB(80, 80, 110));
    TextOutA(g_backbuffer_dc, 20, 370, footer, (int)strlen(footer));

    // Present
    BitBlt(g_hdc, 0, 0, g_display_w, g_display_h,
           g_backbuffer_dc, 0, 0, SRCCOPY);
}

// ---------------------------------------------------------------------------
//  storm_exit — Cleanup
// ---------------------------------------------------------------------------

void storm_exit(void) {
    if (g_backbuffer) { DeleteObject(g_backbuffer); g_backbuffer = NULL; }
    if (g_backbuffer_dc) { DeleteDC(g_backbuffer_dc); g_backbuffer_dc = NULL; }
    if (g_hdc) { ReleaseDC(g_hwnd, g_hdc); g_hdc = NULL; }
    if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = NULL; }
}
