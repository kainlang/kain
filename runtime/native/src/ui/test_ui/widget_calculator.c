// ============================================================================
//  widget_calculator.c — Calculator Widget Demo
//  ============================================================================
//  Demonstrates:
//    - Full 4-function calculator (+ - × ÷) with widget-style buttons
//    - Number buttons 0-9 with decimal point
//    - Operator buttons: +, -, ×, ÷
//    - Clear (C) and Equals (=) buttons
//    - Display panel showing current value and pending operation
//    - Keyboard input support (type numbers/operators)
//    - Visual feedback: highlighted button on hover/click
//    - Proper arithmetic with error handling (divide by zero)
//    - Status bar showing keyboard shortcuts
//  ============================================================================
//  Build:
//    clang -std=c11 -g -O0 widget_calculator.c stubs.c ^
//      ..\ui_system.c ..\ui_host_adapter.c ..\ui_renderer.c ..\ui_layout.c ..\ui_color.c ^
//      ..\..\core\input_system.c ..\..\core\component_surface.c ^
//      -I ..\..\..\include -I .. -I ..\..\core ^
//      -luser32 -lgdi32 -lopengl32 ^
//      -o widget_calculator.exe
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
#define C_SURFACE2  0xFF2E2E48
#define C_ACCENT    0xFF21D4A1
#define C_ACCENT2   0xFF4A90D9
#define C_ACCENT3   0xFFE8914A
#define C_ACCENT4   0xFFE84A5F
#define C_TEXT      0xFFE8E8F0
#define C_TEXT_DIM  0xFF8888A0
#define C_HEADER    0xFF1E1E32
#define C_BUTTON    0xFF303050
#define C_BUTTON_HL 0xFF505078
#define C_BUTTON_PR 0xFF616190
#define C_DISPLAY   0xFF0A0A14
#define C_DISPLAY_B 0xFF2A2A44

// ── Calculator state ───────────────────────────────────────────────────
typedef enum {
    OP_NONE, OP_ADD, OP_SUB, OP_MUL, OP_DIV
} CalcOp;

typedef struct {
    double display_value;
    double memory;
    CalcOp pending_op;
    int new_input;
    int has_error;
    char display_text[64];
} CalcState;

static CalcState g_calc;
static int g_highlight_btn = -1;
static int g_pressed_btn = -1;

// ── Calculator logic ───────────────────────────────────────────────────
static void calc_update_display(void) {
    if (g_calc.has_error) {
        snprintf(g_calc.display_text, sizeof(g_calc.display_text), "Error");
    } else {
        double v = g_calc.display_value;
        if (v == (double)(int64_t)v && fabs(v) < 1e12) {
            snprintf(g_calc.display_text, sizeof(g_calc.display_text), "%lld",
                     (long long)v);
        } else {
            snprintf(g_calc.display_text, sizeof(g_calc.display_text), "%.8f", v);
            // Trim trailing zeros
            char* dot = strchr(g_calc.display_text, '.');
            if (dot) {
                char* end = g_calc.display_text + strlen(g_calc.display_text) - 1;
                while (end > dot && *end == '0') end--;
                if (end == dot) end--;
                *(end + 1) = '\0';
            }
        }
    }
}

static void calc_input_digit(int d) {
    if (g_calc.has_error) {
        g_calc.display_value = 0;
        g_calc.has_error = 0;
    }
    if (g_calc.new_input) {
        g_calc.display_value = 0;
        g_calc.new_input = 0;
    }
    g_calc.display_value = g_calc.display_value * 10.0 + d;
    if (g_calc.display_value > 999999999.0) g_calc.display_value = 999999999.0;
    calc_update_display();
}

static void calc_input_decimal(void) {
    if (g_calc.has_error) {
        g_calc.display_value = 0;
        g_calc.has_error = 0;
    }
    if (g_calc.new_input) {
        g_calc.display_value = 0.0;
        g_calc.new_input = 0;
    }
    // If no decimal point yet, add one by making it a fractional
    if (g_calc.display_value == (double)(int64_t)g_calc.display_value) {
        // Just leave it as-is; next digit will go after decimal
        g_calc.display_value = (double)(int64_t)g_calc.display_value;
    }
    calc_update_display();
    // Append decimal point in display
    if (!strchr(g_calc.display_text, '.')) {
        strcat(g_calc.display_text, ".");
    }
}

static void calc_set_op(CalcOp op);
static void calc_equals(void);

static void calc_set_op(CalcOp op) {
    if (g_calc.has_error) return;
    if (g_calc.pending_op != OP_NONE && !g_calc.new_input) {
        calc_equals();
    }
    g_calc.memory = g_calc.display_value;
    g_calc.pending_op = op;
    g_calc.new_input = 1;
}

static void calc_equals(void) {
    if (g_calc.has_error || g_calc.pending_op == OP_NONE) return;
    double a = g_calc.memory;
    double b = g_calc.display_value;
    switch (g_calc.pending_op) {
        case OP_ADD: g_calc.display_value = a + b; break;
        case OP_SUB: g_calc.display_value = a - b; break;
        case OP_MUL: g_calc.display_value = a * b; break;
        case OP_DIV:
            if (b == 0.0) { g_calc.has_error = 1; }
            else { g_calc.display_value = a / b; }
            break;
        default: break;
    }
    g_calc.pending_op = OP_NONE;
    g_calc.new_input = 1;
    calc_update_display();
}

static void calc_clear(void) {
    g_calc.display_value = 0;
    g_calc.memory = 0;
    g_calc.pending_op = OP_NONE;
    g_calc.new_input = 1;
    g_calc.has_error = 0;
    calc_update_display();
}

// ── Button layout (widget-style regions) ──────────────────────────────
#define NUM_BUTTONS 20

typedef struct {
    double x, y, w, h;
    const char* label;
    int kind;   // 0=digit, 1=operator, 2=equals, 3=clear, 4=decimal
    int digit_value;
    CalcOp op_value;
} CalcButton;

static CalcButton g_buttons[NUM_BUTTONS];
static int g_button_count = 0;

static void layout_calc_buttons(double area_x, double area_y, double area_w, double area_h) {
    g_button_count = 0;
    int cols = 4, rows = 5;
    double gap = 4;
    double bw = (area_w - gap * (cols - 1)) / cols;
    double bh = (area_h - gap * (rows - 1)) / rows;

    // Button definitions: [label, kind, digit/op]
    // kind: 0=digit, 1=operator, 2=equals, 3=clear, 4=decimal
    // Row 0: 7 8 9 ÷
    // Row 1: 4 5 6 ×
    // Row 2: 1 2 3 -
    // Row 3: 0 . C +
    // Row 4: (empty) (empty) (empty) =
    struct { const char* label; int kind; int val; } btn_defs[NUM_BUTTONS] = {
        {"7", 0, 7}, {"8", 0, 8}, {"9", 0, 9}, {"\xf7", 1, OP_DIV},
        {"4", 0, 4}, {"5", 0, 5}, {"6", 0, 6}, {"\xd7", 1, OP_MUL},
        {"1", 0, 1}, {"2", 0, 2}, {"3", 0, 3}, {"-", 1, OP_SUB},
        {"0", 0, 0}, {".", 4, 0},  {"C", 3, 0},  {"+", 1, OP_ADD},
        {"", 0, 0},  {"", 0, 0},   {"", 0, 0},   {"=", 2, 0},
    };

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int idx = r * cols + c;
            CalcButton* b = &g_buttons[idx];
            b->x = area_x + c * (bw + gap);
            b->y = area_y + r * (bh + gap);
            b->w = bw;
            b->h = bh;
            b->label = btn_defs[idx].label;
            b->kind = btn_defs[idx].kind;
            b->digit_value = (btn_defs[idx].kind == 0) ? btn_defs[idx].val : 0;
            b->op_value = (btn_defs[idx].kind == 1) ? (CalcOp)btn_defs[idx].val : OP_NONE;
            g_button_count++;
        }
    }
}

static int hit_test_button(double mx, double my) {
    for (int i = 0; i < g_button_count; i++) {
        CalcButton* b = &g_buttons[i];
        if (b->label[0] == '\0') continue; // skip empty slots
        if (mx >= b->x && mx < b->x + b->w && my >= b->y && my < b->y + b->h) {
            return i;
        }
    }
    return -1;
}

static void handle_button_press(int idx) {
    if (idx < 0 || idx >= g_button_count) return;
    CalcButton* b = &g_buttons[idx];

    switch (b->kind) {
        case 0: // Digit
            calc_input_digit(b->digit_value);
            break;
        case 1: // Operator
            calc_set_op(b->op_value);
            break;
        case 2: // Equals
            calc_equals();
            break;
        case 3: // Clear
            calc_clear();
            break;
        case 4: // Decimal
            calc_input_decimal();
            break;
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

// ── Paint calculator ──────────────────────────────────────────────────
static void paint_calculator(uint32_t* fb, int w, int h, int stride, HDC gdi_dc) {
    // Clear background
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++)
            fb[r * stride + c] = C_BG;

    int pad = 12;
    int header_h = 50;

    // ── Header bar ─────────────────────────────────────────────────
    fill_rect(fb, stride, 0, 0, w, header_h, C_HEADER);
    fill_rect(fb, stride, 0, header_h - 2, w, 2, C_ACCENT);

    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0xE8, 0xE8, 0xF0));
        SetBkMode(gdi_dc, TRANSPARENT);
        SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
        TextOutA(gdi_dc, 14, 8, "Calculator", 10);
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        TextOutA(gdi_dc, 14, 26, "Kain Native UI — Widget Demo", 28);
    }

    // ── Display area ───────────────────────────────────────────────
    int disp_y = header_h + pad;
    int disp_h = 70;
    int disp_x = pad;
    int disp_w = w - 2 * pad;

    // Outer border
    fill_rounded_rect(fb, stride, w, h, disp_x, disp_y, disp_w, disp_h, C_DISPLAY_B, 6);
    // Inner fill
    fill_rounded_rect(fb, stride, w, h, disp_x + 1, disp_y + 1,
                      disp_w - 2, disp_h - 2, C_DISPLAY, 5);

    // Operator indicator
    if (g_calc.pending_op != OP_NONE && gdi_dc) {
        const char* op_chars = "+-\xd7\xf7";
        char op_str[2] = {op_chars[g_calc.pending_op - 1], '\0'};
        SetTextColor(gdi_dc, RGB(0x21, 0xD4, 0xA1));
        SetBkMode(gdi_dc, TRANSPARENT);
        SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
        TextOutA(gdi_dc, disp_x + 10, disp_y + 6, op_str, 1);

        // Show memory value
        char mem_str[32];
        snprintf(mem_str, sizeof(mem_str), "%.0f", g_calc.memory);
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        TextOutA(gdi_dc, disp_x + 26, disp_y + 6, mem_str, (int)strlen(mem_str));
    }

    // Display value text
    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0xE8, 0xE8, 0xF0));
        SetBkMode(gdi_dc, TRANSPARENT);
        HFONT hf = CreateFontA(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                               DEFAULT_PITCH, "Consolas");
        SelectObject(gdi_dc, hf);

        RECT text_r = {
            disp_x + 8, disp_y + 8,
            disp_x + disp_w - 8, disp_y + disp_h - 8
        };
        DrawTextA(gdi_dc, g_calc.display_text, -1, &text_r,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        DeleteObject(hf);
    }

    // ── Error indicator ────────────────────────────────────────────
    if (g_calc.has_error && gdi_dc) {
        SetTextColor(gdi_dc, RGB(0xE8, 0x4A, 0x5F));
        SetBkMode(gdi_dc, TRANSPARENT);
        SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
        TextOutA(gdi_dc, disp_x + 8, disp_y + disp_h + 4, "Divide by zero!", 15);
    }

    // ── Button grid ───────────────────────────────────────────────
    int btn_area_y = disp_y + disp_h + pad;
    int btn_area_h = h - btn_area_y - pad - 28; // leave room for status bar
    layout_calc_buttons(pad, btn_area_y, w - 2 * pad, btn_area_h);

    // Draw buttons
    HFONT digit_font = CreateFontA(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                    CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                    DEFAULT_PITCH, "Segoe UI");
    HFONT op_font = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                 DEFAULT_PITCH, "Segoe UI");

    for (int i = 0; i < g_button_count; i++) {
        CalcButton* b = &g_buttons[i];
        if (b->label[0] == '\0') continue;

        int bx = (int)b->x, by = (int)b->y, bw = (int)b->w, bh = (int)b->h;

        // Determine colors
        uint32_t btn_color;
        uint32_t text_color = 0xFFFFFFFF;

        if (i == g_pressed_btn) {
            btn_color = C_BUTTON_PR;
        } else if (i == g_highlight_btn) {
            btn_color = C_BUTTON_HL;
        } else if (b->kind == 1 || b->kind == 2) {
            // Operators and equals
            btn_color = C_ACCENT2;
        } else if (b->kind == 3) {
            // Clear
            btn_color = C_ACCENT4;
        } else if (b->kind == 4) {
            // Decimal
            btn_color = C_BUTTON;
        } else {
            btn_color = C_BUTTON;
            text_color = C_TEXT;
        }

        fill_rounded_rect(fb, stride, w, h, bx, by, bw, bh, btn_color, 6);
        // Subtle inner border
        fill_rounded_rect(fb, stride, w, h, bx + 1, by + 1, bw - 2, bh - 2,
                          ui_color_blend(0x50000000, btn_color), 5);

        // Button text via GDI
        if (gdi_dc) {
            SetTextColor(gdi_dc, RGB((text_color >> 16) & 0xFF,
                                     (text_color >> 8) & 0xFF,
                                     text_color & 0xFF));
            SetBkMode(gdi_dc, TRANSPARENT);
            SelectObject(gdi_dc, (b->kind == 1 || b->kind == 2) ? op_font : digit_font);

            RECT btn_r = { bx, by, bx + bw, by + bh };
            DrawTextA(gdi_dc, b->label, -1, &btn_r,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    DeleteObject(digit_font);
    DeleteObject(op_font);

    // ── Status bar ─────────────────────────────────────────────────
    int status_y = h - 24;
    fill_rect(fb, stride, 0, status_y, w, 24, C_HEADER);
    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
        TextOutA(gdi_dc, 10, status_y + 4,
                 "Keyboard: 0-9 . + - * / Enter Esc C  |  Click buttons to calculate",
                 61);
    }
}

// ── Window subclass ────────────────────────────────────────────────────
static WNDPROC g_orig_wndproc = NULL;

static LRESULT CALLBACK calc_window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
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
            int btn = hit_test_button((double)mx, (double)my);
            if (btn >= 0) {
                g_highlight_btn = btn;
                g_pressed_btn = btn;
                handle_button_press(btn);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            g_pressed_btn = -1;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE: {
            int mx = (int)(short)LOWORD(lp);
            int my = (int)(short)HIWORD(lp);
            int btn = hit_test_button((double)mx, (double)my);
            if (btn != g_highlight_btn) {
                g_highlight_btn = btn;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
        }
        case WM_KEYDOWN: {
            int vk = (int)wp;
            if (vk >= '0' && vk <= '9') {
                calc_input_digit(vk - '0');
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (vk == VK_OEM_PLUS || vk == VK_ADD) {
                calc_set_op(OP_ADD);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (vk == VK_OEM_MINUS || vk == VK_SUBTRACT) {
                calc_set_op(OP_SUB);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (vk == VK_MULTIPLY) {
                calc_set_op(OP_MUL);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (vk == VK_DIVIDE) {
                calc_set_op(OP_DIV);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (vk == VK_RETURN) {
                calc_equals();
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (vk == VK_ESCAPE) {
                PostQuitMessage(0);
            } else if (vk == VK_BACK || vk == 'C') {
                calc_clear();
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (vk == VK_OEM_PERIOD || vk == VK_DECIMAL) {
                calc_input_decimal();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_CHAR: {
            char ch = (char)wp;
            if (ch == 'c' || ch == 'C') {
                calc_clear();
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (ch == '+' || ch == '=') {
                if (ch == '+') calc_set_op(OP_ADD);
                else calc_equals();
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (ch == '-') {
                calc_set_op(OP_SUB);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (ch == '*') {
                calc_set_op(OP_MUL);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (ch == '/') {
                calc_set_op(OP_DIV);
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (ch == '.') {
                calc_input_decimal();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
    }
    return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
}

// ── Main ───────────────────────────────────────────────────────────────
int main(void) {
    int win_w = 420, win_h = 560;

    printf("=== Widget Calculator — Kain Native UI ===\n");
    printf("Build: " __DATE__ " " __TIME__ "\n\n");

    // Init calculator state
    calc_clear();

    // Init UI
    abi_ui_reset();
    int64_t session = abi_ui_session_create("WidgetCalc", win_w, win_h);
    if (session <= 0) { fprintf(stderr, "FAIL: session_create\n"); return 1; }

    abi_ui_window_open(session, "Calculator — Kain Native UI Widget Demo", win_w, win_h);
    if (abi_ui_host_attach(session, "winit") != 0) {
        fprintf(stderr, "FAIL: host_attach\n"); return 1;
    }
    printf("Session: %lld  Backend: %s\n", (long long)session,
           abi_ui_host_backend(session));

    KainNativeUiSession* ks = abi_ui_find_session(session);
    if (!ks || !ks->host_state) { fprintf(stderr, "FAIL: no host state\n"); return 1; }
    KainWin32UiHost* host = (KainWin32UiHost*)ks->host_state;

    // Subclass window
    g_orig_wndproc = (WNDPROC)SetWindowLongPtrA(host->hwnd, GWLP_WNDPROC,
                                                  (LONG_PTR)calc_window_proc);
    printf("Window: hwnd=%p  fb=%p  %dx%d\n",
           (void*)host->hwnd, (void*)host->framebuffer, host->width, host->height);

    // Build minimal node tree
    int64_t root = abi_ui_node_create(session, "window");
    abi_ui_node_set_rect(session, root, 0, 0, win_w, win_h);

    int64_t bg = abi_ui_node_create(session, "bg");
    abi_ui_node_set_parent(session, bg, root);
    abi_ui_node_set_rect(session, bg, 0, 0, win_w, win_h);
    abi_ui_node_set_style_string(session, bg, "fill_color", "#1A1A24");

    printf("\nFrame loop running. Type numbers and operators or click buttons.\n");
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

        // Render
        if (host->framebuffer) {
            paint_calculator((uint32_t*)host->framebuffer,
                            host->width, host->height, host->fb_stride / 4,
                            host->hdc_buffer);
            InvalidateRect(host->hwnd, NULL, FALSE);
        }

        frame++;
        if (frame % 60 == 0) {
            printf("Frame %lld | display='%s' | fb[0]=0x%08X\n",
                   (long long)frame, g_calc.display_text,
                   host->framebuffer ? *(uint32_t*)host->framebuffer : 0);
        }

        Sleep(16);
    }

    printf("\nShutdown after %lld frames.\n", (long long)frame);
    printf("Final display value: %s\n", g_calc.display_text);
    abi_ui_session_destroy(session);
    printf("Done.\n");
    return 0;
}
