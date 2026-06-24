// ============================================================================
//  keypad.c — PIN Entry Keypad
//  ============================================================================
//  Demonstrates:
//    - 10 digit buttons (0-9) as styled clickable panels
//    - Enter and Clear buttons
//    - Visual feedback: highlighted button on press
//    - Display showing entered digits as ●●●● (masked)
//    - Max 6 digits with visual overflow indicator
//    - Keyboard input support
//  ============================================================================
//  Build:
//    clang -std=c11 -g -O0 keypad.c ../TEST/stubs.c ^
//      ../ui_system.c ../ui_host_adapter.c ../ui_renderer.c ../ui_layout.c ../ui_color.c ^
//      ../../core/input_system.c ^
//      -I../../../include -I.. -I../../core ^
//      -luser32 -lgdi32 -lopengl32 -o keypad.exe
//  ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ui_system.h"
#include "ui_system_internal.h"
#include "ui_host_adapter.h"
#include "../../include/ui_renderer.h"
#include "../../include/ui_layout.h"
#include "../../include/ui_color.h"

// ── Stubs ──────────────────────────────────────────────────────────────
char* string_new(char* src);
double kain_clampd(double value, double min_value, double max_value);

// ── KainWin32UiHost ────────────────────────────────────────────────────
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

// ── Color palette ──────────────────────────────────────────────────────
#define C_BG        0xFF1A1A24
#define C_SURFACE   0xFF252540
#define C_ACCENT    0xFF21D4A1
#define C_ACCENT2   0xFF4A90D9
#define C_ACCENT4   0xFFE84A5F
#define C_TEXT      0xFFE8E8F0
#define C_TEXT_DIM  0xFF8888A0
#define C_HEADER    0xFF1E1E32
#define C_BUTTON    0xFF303050
#define C_BUTTON_HL 0xFF21D4A1
#define C_DISPLAY   0xFF0A0A14
#define C_GREEN_OK  0xFF21D4A1
#define C_RED_ERR   0xFFE84A5F

// ── PIN state ──────────────────────────────────────────────────────────
#define MAX_PIN_DIGITS 6

static char g_pin_digits[MAX_PIN_DIGITS + 1] = {0};
static int g_pin_length = 0;
static int g_pin_entered = 0;     // -1=wrong, 1=correct
static int g_highlight_btn = -1;
static int g_message_timer = 0;   // frames remaining for message display

static const char* CORRECT_PIN = "1234";

static void pin_clear(void) {
    memset(g_pin_digits, 0, sizeof(g_pin_digits));
    g_pin_length = 0;
    g_pin_entered = 0;
    g_message_timer = 0;
}

static void pin_add_digit(int d) {
    if (g_pin_entered != 0) pin_clear();
    if (g_pin_length >= MAX_PIN_DIGITS) return;
    g_pin_digits[g_pin_length++] = '0' + d;
    g_pin_digits[g_pin_length] = '\0';
}

static void pin_submit(void) {
    if (g_pin_length == 0) return;
    if (strcmp(g_pin_digits, CORRECT_PIN) == 0) {
        g_pin_entered = 1;
    } else {
        g_pin_entered = -1;
    }
    g_message_timer = 120; // show message for 120 frames (~2 seconds)
}

// ── Button layout ──────────────────────────────────────────────────────
#define KEYPAD_COLS 3
#define KEYPAD_ROWS 4

typedef struct {
    double x, y, w, h;
    const char* label;
    int value;   // 0-9 for digits, 10=clear, 11=enter
} KeyButton;

static KeyButton g_key_buttons[12];
static int g_key_count = 0;

static void layout_keypad(double area_x, double area_y, double area_w, double area_h) {
    g_key_count = 0;
    double gap = 6;
    double bw = (area_w - gap * (KEYPAD_COLS - 1)) / KEYPAD_COLS;
    double bh = (area_h - gap * (KEYPAD_ROWS - 1)) / KEYPAD_ROWS;

    // Button labels and values in grid order (left to right, top to bottom)
    const char* labels[] = {
        "1", "2", "3",
        "4", "5", "6",
        "7", "8", "9",
        "Clear", "0", "Enter"
    };
    int values[] = {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9,
        10, 0, 11
    };

    for (int r = 0; r < KEYPAD_ROWS; r++) {
        for (int c = 0; c < KEYPAD_COLS; c++) {
            int idx = r * KEYPAD_COLS + c;
            KeyButton* b = &g_key_buttons[idx];
            b->x = area_x + c * (bw + gap);
            b->y = area_y + r * (bh + gap);
            b->w = bw;
            b->h = bh;
            b->label = labels[idx];
            b->value = values[idx];
            g_key_count++;
        }
    }
}

static int hit_test_key(double mx, double my) {
    for (int i = 0; i < g_key_count; i++) {
        KeyButton* b = &g_key_buttons[i];
        if (mx >= b->x && mx < b->x + b->w && my >= b->y && my < b->y + b->h) {
            return i;
        }
    }
    return -1;
}

static void handle_key_press(int idx) {
    if (idx < 0 || idx >= g_key_count) return;
    KeyButton* b = &g_key_buttons[idx];

    if (b->value >= 0 && b->value <= 9) {
        pin_add_digit(b->value);
    } else if (b->value == 10) {
        pin_clear();
    } else if (b->value == 11) {
        pin_submit();
    }
}

// ── Pixel helpers ──────────────────────────────────────────────────────
static void fill_rect(uint32_t* fb, int stride, int x, int y, int w, int h, uint32_t color) {
    for (int r = y; r < y + h && r < 2000; r++)
        for (int c = x; c < x + w && c < 2000; c++)
            if (r >= 0 && c >= 0) fb[r * stride + c] = color;
}

static void fill_rounded_rect(uint32_t* fb, int stride, int fb_w, int fb_h,
                              int x, int y, int w, int h, uint32_t color, int radius) {
    if (radius <= 0) { fill_rect(fb, stride, x, y, w, h, color); return; }
    int r2 = radius * radius;
    for (int row = y; row < y + h && row < fb_h; row++) {
        for (int col = x; col < x + w && col < fb_w; col++) {
            if (row < 0 || col < 0) continue;
            int inside = 1;
            if (col < x + radius && row < y + radius) {
                int dx = (x + radius) - col, dy = (y + radius) - row;
                inside = (dx*dx + dy*dy) <= r2;
            } else if (col >= x + w - radius && row < y + radius) {
                int dx = col - (x + w - radius), dy = (y + radius) - row;
                inside = (dx*dx + dy*dy) <= r2;
            } else if (col < x + radius && row >= y + h - radius) {
                int dx = (x + radius) - col, dy = row - (y + h - radius);
                inside = (dx*dx + dy*dy) <= r2;
            } else if (col >= x + w - radius && row >= y + h - radius) {
                int dx = col - (x + w - radius), dy = row - (y + h - radius);
                inside = (dx*dx + dy*dy) <= r2;
            }
            if (inside) fb[row * stride + col] = color;
        }
    }
}

// ── Paint the keypad UI ────────────────────────────────────────────────
static void paint_keypad(uint32_t* fb, int w, int h, int stride, HDC gdi_dc) {
    // Clear background
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++)
            fb[r * stride + c] = C_BG;

    int pad = 20;
    int header_h = 50;

    // ── Header ─────────────────────────────────────────────────────
    fill_rect(fb, stride, 0, 0, w, header_h, C_HEADER);
    fill_rect(fb, stride, 0, header_h - 2, w, 2, C_ACCENT);

    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0xE8, 0xE8, 0xF0));
        SetBkMode(gdi_dc, TRANSPARENT);
        SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
        TextOutA(gdi_dc, 14, 6, "PIN Entry Keypad", 16);
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        TextOutA(gdi_dc, 14, 26, "Kain Native UI  |  TEST_V2", 26);
    }

    // ── PIN display area ────────────────────────────────────────────
    int disp_y = header_h + pad;
    int disp_h = 80;
    int disp_w = w - 2 * pad;
    int disp_x = pad;

    // Display panel background
    fill_rounded_rect(fb, stride, w, h, disp_x, disp_y, disp_w, disp_h, 0xFF0A0A14, 8);
    // Border
    fill_rounded_rect(fb, stride, w, h, disp_x, disp_y, disp_w, disp_h, 0xFF3A3A5C, 8);
    fill_rounded_rect(fb, stride, w, h, disp_x + 1, disp_y + 1, disp_w - 2, disp_h - 2, 0xFF0A0A14, 7);

    // PIN dots
    if (gdi_dc) {
        // Draw masked digits as ● symbols
        int dot_size = 20;
        int dot_gap = 12;
        int total_w = MAX_PIN_DIGITS * dot_size + (MAX_PIN_DIGITS - 1) * dot_gap;
        int start_x = disp_x + (disp_w - total_w) / 2;
        int dot_y = disp_y + (disp_h - dot_size) / 2;

        // Determine dot color based on state
        COLORREF dot_color;
        if (g_pin_entered == 1) {
            dot_color = RGB(0x21, 0xD4, 0xA1); // green
        } else if (g_pin_entered == -1) {
            dot_color = RGB(0xE8, 0x4A, 0x5F); // red
        } else {
            dot_color = RGB(0xE8, 0xE8, 0xF0); // white
        }

        SetTextColor(gdi_dc, dot_color);
        HFONT dot_font = CreateFontA(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                      CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                      DEFAULT_PITCH, "Segoe UI");
        SelectObject(gdi_dc, dot_font);

        for (int i = 0; i < MAX_PIN_DIGITS; i++) {
            int dx = start_x + i * (dot_size + dot_gap);
            if (i < g_pin_length) {
                // Show ● for entered digit
                TextOutA(gdi_dc, dx + 2, dot_y - 2, "\x95", 1); // bullet character
            } else {
                // Show ○ for empty slot
                TextOutA(gdi_dc, dx + 2, dot_y - 2, "\xBA", 1); // empty circle
            }
        }
        DeleteObject(dot_font);

        // Message text
        if (g_message_timer > 0) {
            const char* msg;
            COLORREF msg_color;
            if (g_pin_entered == 1) {
                msg = "ACCESS GRANTED";
                msg_color = RGB(0x21, 0xD4, 0xA1);
            } else if (g_pin_entered == -1) {
                msg = "ACCESS DENIED";
                msg_color = RGB(0xE8, 0x4A, 0x5F);
            } else {
                msg = "";
                msg_color = RGB(0x88, 0x88, 0xA0);
            }
            if (msg[0]) {
                SetTextColor(gdi_dc, msg_color);
                SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
                RECT msg_r = { disp_x, disp_y + disp_h + 4, disp_x + disp_w, disp_y + disp_h + 24 };
                DrawTextA(gdi_dc, msg, -1, &msg_r, DT_CENTER);
            }
        } else if (g_pin_length > 0 && g_pin_entered == 0) {
            SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
            char len_str[32];
            snprintf(len_str, sizeof(len_str), "%d / %d digits", g_pin_length, MAX_PIN_DIGITS);
            RECT msg_r = { disp_x, disp_y + disp_h + 4, disp_x + disp_w, disp_y + disp_h + 24 };
            DrawTextA(gdi_dc, len_str, -1, &msg_r, DT_CENTER);
        }
    }

    // ── Keypad grid ────────────────────────────────────────────────
    int key_area_y = disp_y + disp_h + 36;
    int key_area_h = h - key_area_y - pad - 30; // leave room for status
    layout_keypad(pad + 20, key_area_y, w - 2 * (pad + 20), key_area_h);

    // Draw keys
    HFONT key_font = CreateFontA(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                  DEFAULT_PITCH, "Segoe UI");

    for (int i = 0; i < g_key_count; i++) {
        KeyButton* b = &g_key_buttons[i];
        int bx = (int)b->x, by = (int)b->y, bw = (int)b->w, bh = (int)b->h;

        uint32_t btn_color;
        uint32_t text_color;
        if (i == g_highlight_btn) {
            btn_color = C_BUTTON_HL;
            text_color = 0xFFFFFFFF;
        } else if (b->value == 10) { // Clear
            btn_color = 0xFFE84A5F;
            text_color = 0xFFFFFFFF;
        } else if (b->value == 11) { // Enter
            btn_color = 0xFF21D4A1;
            text_color = 0xFFFFFFFF;
        } else {
            btn_color = C_BUTTON;
            text_color = C_TEXT;
        }

        fill_rounded_rect(fb, stride, w, h, bx, by, bw, bh, btn_color, 8);
        // Subtle inner border
        fill_rounded_rect(fb, stride, w, h, bx + 1, by + 1, bw - 2, bh - 2,
                          ui_color_blend(0x40000000, btn_color), 7);

        if (gdi_dc && b->label[0]) {
            SetTextColor(gdi_dc, RGB((text_color >> 16) & 0xFF,
                                     (text_color >> 8) & 0xFF,
                                     text_color & 0xFF));
            SetBkMode(gdi_dc, TRANSPARENT);
            SelectObject(gdi_dc, key_font);
            RECT btn_r = { bx, by, bx + bw, by + bh };
            DrawTextA(gdi_dc, b->label, -1, &btn_r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
    DeleteObject(key_font);

    // ── Status bar ─────────────────────────────────────────────────
    int status_y = h - 24;
    fill_rect(fb, stride, 0, status_y, w, 24, C_HEADER);
    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
        TextOutA(gdi_dc, 10, status_y + 4, "PIN: 1234  |  Click keys or type digits  |  Esc to exit", 58);
    }
}

// ── Window subclass ────────────────────────────────────────────────────
static WNDPROC g_orig_wndproc = NULL;

static LRESULT CALLBACK keypad_window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (hdc) {
                KainWin32UiHost* host = (KainWin32UiHost*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
                if (host && host->hdc_buffer) {
                    BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
                           ps.rcPaint.right - ps.rcPaint.left,
                           ps.rcPaint.bottom - ps.rcPaint.top,
                           host->hdc_buffer, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
                }
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int mx = (int)(short)LOWORD(lp);
            int my = (int)(short)HIWORD(lp);
            int btn = hit_test_key((double)mx, (double)my);
            if (btn >= 0) {
                g_highlight_btn = btn;
                handle_key_press(btn);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            g_highlight_btn = -1;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_KEYDOWN: {
            int vk = (int)wp;
            if (vk >= '0' && vk <= '9') {
                pin_add_digit(vk - '0');
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (vk == VK_RETURN) {
                pin_submit();
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (vk == VK_BACK || vk == VK_ESCAPE) {
                if (vk == VK_ESCAPE) { PostQuitMessage(0); }
                else { pin_clear(); InvalidateRect(hwnd, NULL, FALSE); }
            }
            return 0;
        }
    }
    return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
}

// ── Main ───────────────────────────────────────────────────────────────
int main(void) {
    int win_w = 380, win_h = 540;

    printf("=== PIN Entry Keypad — Kain Native UI ===\n");
    printf("Build: " __DATE__ " " __TIME__ "\n");
    printf("Correct PIN: %s\n\n", CORRECT_PIN);

    // Init
    pin_clear();

    abi_ui_reset();
    int64_t session = abi_ui_session_create("Keypad", win_w, win_h);
    if (session <= 0) { fprintf(stderr, "FAIL: session_create\n"); return 1; }

    abi_ui_window_open(session, "PIN Entry Keypad — Kain Native UI", win_w, win_h);
    if (abi_ui_host_attach(session, "winit") != 0) {
        fprintf(stderr, "FAIL: host_attach\n"); return 1;
    }

    KainNativeUiSession* ks = abi_ui_find_session(session);
    if (!ks || !ks->host_state) { fprintf(stderr, "FAIL: no host state\n"); return 1; }
    KainWin32UiHost* host = (KainWin32UiHost*)ks->host_state;

    // Subclass
    g_orig_wndproc = (WNDPROC)SetWindowLongPtrA(host->hwnd, GWLP_WNDPROC,
                                                  (LONG_PTR)keypad_window_proc);
    printf("Window: %dx%d  hwnd=%p\n", host->width, host->height, (void*)host->hwnd);

    // Build minimal node tree
    int64_t root = abi_ui_node_create(session, "root");
    abi_ui_node_set_rect(session, root, 0, 0, win_w, win_h);
    int64_t bg = abi_ui_node_create(session, "bg");
    abi_ui_node_set_parent(session, bg, root);
    abi_ui_node_set_rect(session, bg, 0, 0, win_w, win_h);
    abi_ui_node_set_style_string(session, bg, "fill_color", "#1A1A24");

    printf("\nFrame loop running. Correct PIN: %s\n", CORRECT_PIN);
    printf("Type digits or click buttons. Enter to submit. Esc to exit.\n");
    printf("========================================================\n");

    int64_t frame = 0;
    MSG msg;

    while (1) {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { host->running = 0; break; }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!host->running) break;

        abi_ui_begin_frame(session, 16.67);
        abi_ui_end_frame(session);

        // Decrement message timer
        if (g_message_timer > 0) {
            g_message_timer--;
            if (g_message_timer == 0 && g_pin_entered != 0) {
                pin_clear();
            }
        }

        // Render
        if (host->framebuffer) {
            paint_keypad((uint32_t*)host->framebuffer,
                        host->width, host->height, host->fb_stride / 4,
                        host->hdc_buffer);
            InvalidateRect(host->hwnd, NULL, FALSE);
        }

        frame++;
        if (frame % 60 == 0) {
            printf("Frame %lld | PIN: %s | State: %s\n",
                   (long long)frame,
                   g_pin_length > 0 ? "****" : "(empty)",
                   g_pin_entered == 1 ? "GRANTED" : (g_pin_entered == -1 ? "DENIED" : "waiting"));
        }

        Sleep(16);
    }

    printf("\nShutdown after %lld frames.\n", (long long)frame);
    abi_ui_session_destroy(session);
    printf("Done.\n");
    return 0;
}
