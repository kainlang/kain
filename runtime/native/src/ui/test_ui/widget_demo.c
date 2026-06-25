// ============================================================================
//  widget_demo.c — Comprehensive Widget System Demo
//  ============================================================================
//  Demonstrates:
//    - 5 panels arranged vertically, each showing one widget type:
//      Panel 1: Buttons — click counter, color toggle, exit
//      Panel 2: Checkboxes — settings toggles (sound, music, effects)
//      Panel 3: Sliders — volume, brightness, speed (draggable thumbs)
//      Panel 4: Text input — name entry form with live preview
//      Panel 5: Progress bars — simulated concurrent downloads
//    - Status bar showing FPS, active widget info, frame count
//    - Window subclass for mouse and keyboard input
//    - Direct framebuffer rendering with GDI text
//  ============================================================================
//  Build:
//    clang -std=c11 -g -O0 widget_demo.c stubs.c ^
//      ..\ui_system.c ..\ui_host_adapter.c ..\ui_renderer.c ..\ui_layout.c ..\ui_color.c ^
//      ..\..\core\input_system.c ..\..\core\component_surface.c ^
//      -I ..\..\..\include -I .. -I ..\..\core ^
//      -luser32 -lgdi32 -lopengl32 ^
//      -o widget_demo.exe
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
#define C_BG        0xFF0F172A
#define C_SURFACE   0xFF1E293B
#define C_SURFACE2  0xFF252540
#define C_PANEL     0xFF1A1A2E
#define C_PANEL_BDR 0xFF3A3A5C
#define C_ACCENT    0xFF21D4A1
#define C_ACCENT2   0xFF4A90D9
#define C_ACCENT3   0xFFE8914A
#define C_ACCENT4   0xFFE84A5F
#define C_TEXT      0xFFE8E8F0
#define C_TEXT_DIM  0xFF8888A0
#define C_HEADER    0xFF16162A
#define C_BUTTON    0xFF303050
#define C_BUTTON_HL 0xFF505078
#define C_CHECK_ON  0xFF21D4A1
#define C_CHECK_OFF 0xFF303050
#define C_SLIDER_BG 0xFF2A2A44
#define C_SLIDER_FG 0xFF4A90D9
#define C_SLIDER_TH 0xFFE8E8F0
#define C_PROGRESS  0xFF21D4A1
#define C_INPUT_BG  0xFF0A0A14

// ── Application state ──────────────────────────────────────────────────
// Panel 1: Buttons
static int g_click_count = 0;
static int g_toggle_state = 0;   // 0=normal, 1=alt colors
static int g_highlight_btn = -1;

// Panel 2: Checkboxes
static int g_check_sound = 1;
static int g_check_music = 0;
static int g_check_effects = 1;
static int g_highlight_check = -1;

// Panel 3: Sliders (stored as 0-100 integers)
static int g_slider_volume = 75;
static int g_slider_brightness = 60;
static int g_slider_speed = 30;
static int g_dragging_slider = -1; // -1 = none, 0-2 = which slider
static int g_highlight_slider = -1;

// Panel 4: Text input
static char g_input_buf[64] = "Widget User";
static int g_input_len = 11;
static int g_input_focused = 0;
static int g_cursor_blink = 0;

// Panel 5: Progress bars
static double g_progress[4] = {0.0, 0.0, 0.0, 0.0};
static double g_prog_speed[4] = {0.3, 0.7, 0.15, 0.45};

// General
static double g_fps = 60.0;
static int64_t g_frame_count = 0;
static int64_t g_fps_frames = 0;
static double g_fps_timer = 0.0;
static char g_active_widget[128] = "Ready";

// ── Layout regions (all pre-computed) ──────────────────────────────────
typedef struct {
    double x, y, w, h;
    int type;  // 0=panel header, 1=btn, 2=check, 3=slider, 4=input, 5=progress
    int id;    // panel-relative id
    const char* label;
} Region;

#define MAX_REGIONS 128
static Region g_regions[MAX_REGIONS];
static int g_region_count = 0;

// Panel layout constants
#define PANEL_PAD 10
#define PANEL_GAP 8
#define HEADER_H 30
#define CONTENT_PAD 14

static void add_region(double x, double y, double w, double h,
                       int type, int id, const char* label) {
    if (g_region_count >= MAX_REGIONS) return;
    Region* r = &g_regions[g_region_count++];
    r->x = x; r->y = y; r->w = w; r->h = h;
    r->type = type; r->id = id; r->label = label;
}

static void layout_panels(int win_w, int win_h) {
    g_region_count = 0;
    int margin = 12;
    int panel_w = win_w - 2 * margin;
    int content_w = panel_w - 2 * CONTENT_PAD;
    int y = 56; // below header

    // ── Panel 1: Buttons ──────────────────────────────────────────
    {
        int ph = 90;
        add_region(margin, y, panel_w, ph, 0, 100, "Buttons");
        int cy = y + HEADER_H + 6;
        // Click counter button
        add_region(margin + CONTENT_PAD, cy, 140, 40, 1, 0, "Click Me");
        // Color toggle button
        add_region(margin + CONTENT_PAD + 150, cy, 140, 40, 1, 1, "Toggle Color");
        // Exit button
        add_region(margin + CONTENT_PAD + 300, cy, 100, 40, 1, 2, "Exit");
        y += ph + PANEL_GAP;
    }

    // ── Panel 2: Checkboxes ────────────────────────────────────────
    {
        int ph = 90;
        add_region(margin, y, panel_w, ph, 0, 200, "Settings");
        int cy = y + HEADER_H + 8;
        add_region(margin + CONTENT_PAD, cy, 140, 24, 2, 0, "Sound");
        add_region(margin + CONTENT_PAD, cy + 30, 140, 24, 2, 1, "Music");
        add_region(margin + CONTENT_PAD, cy + 60, 140, 24, 2, 2, "Effects");
        y += ph + PANEL_GAP;
    }

    // ── Panel 3: Sliders ───────────────────────────────────────────
    {
        int ph = 110;
        add_region(margin, y, panel_w, ph, 0, 300, "Sliders");
        int cy = y + HEADER_H + 6;
        int slider_w = content_w - 80;
        int slider_y = cy;
        add_region(margin + CONTENT_PAD + 70, slider_y, slider_w, 20, 3, 0, "Volume");
        add_region(margin + CONTENT_PAD + 70, slider_y + 30, slider_w, 20, 3, 1, "Brightness");
        add_region(margin + CONTENT_PAD + 70, slider_y + 60, slider_w, 20, 3, 2, "Speed");
        // labels for sliders
        add_region(margin + CONTENT_PAD, slider_y, 60, 20, 3, 10, "Volume");
        add_region(margin + CONTENT_PAD, slider_y + 30, 70, 20, 3, 11, "Brightness");
        add_region(margin + CONTENT_PAD, slider_y + 60, 50, 20, 3, 12, "Speed");
        y += ph + PANEL_GAP;
    }

    // ── Panel 4: Text input ────────────────────────────────────────
    {
        int ph = 100;
        add_region(margin, y, panel_w, ph, 0, 400, "Text Input");
        int cy = y + HEADER_H + 8;
        int input_w = content_w;
        add_region(margin + CONTENT_PAD, cy, input_w, 32, 4, 0, g_input_buf);
        // Preview label
        add_region(margin + CONTENT_PAD, cy + 38, input_w, 20, 4, 1, NULL);
        y += ph + PANEL_GAP;
    }

    // ── Panel 5: Progress bars ─────────────────────────────────────
    {
        int ph = 130;
        add_region(margin, y, panel_w, ph, 0, 500, "Downloads");
        int cy = y + HEADER_H + 6;
        int prog_w = content_w - 50;
        int py = cy;
        for (int i = 0; i < 4; i++) {
            char label[32];
            snprintf(label, 32, "File %d", i + 1);
            add_region(margin + CONTENT_PAD, py, 40, 18, 5, 100 + i,
                       _strdup(label)); // leaked but fine for demo
            add_region(margin + CONTENT_PAD + 45, py, prog_w, 18, 5, i, NULL);
            py += 26;
        }
        y += ph + PANEL_GAP;
    }
}

// ── Hit testing ───────────────────────────────────────────────────────
static int hit_test_region(double mx, double my, int* out_type) {
    for (int i = 0; i < g_region_count; i++) {
        Region* r = &g_regions[i];
        if (mx >= r->x && mx < r->x + r->w &&
            my >= r->y && my < r->y + r->h) {
            if (out_type) *out_type = r->type;
            return i;
        }
    }
    return -1;
}

// ── Helper to get slider value from mouse x ────────────────────────────
static int slider_value_from_x(Region* r, double mx) {
    double frac = (mx - r->x) / r->w;
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;
    return (int)(frac * 100.0);
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

static void draw_h_line(uint32_t* fb, int stride, int x, int y, int len, uint32_t color) {
    for (int c = x; c < x + len && c < 4000; c++)
        if (y >= 0 && y < 2000) fb[y * stride + c] = color;
}

// ── Paint the full widget demo ─────────────────────────────────────────
static void paint_demo(uint32_t* fb, int w, int h, int stride, HDC gdi_dc) {
    // Clear background
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++)
            fb[r * stride + c] = C_BG;

    int header_h = 44;
    int status_h = 26;

    // ── Header ─────────────────────────────────────────────────────
    fill_rect(fb, stride, 0, 0, w, header_h, C_HEADER);
    fill_rect(fb, stride, 0, header_h - 2, w, 2, C_ACCENT);

    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0xE8, 0xE8, 0xF0));
        SetBkMode(gdi_dc, TRANSPARENT);
        SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
        TextOutA(gdi_dc, 14, 6, "Widget System Demo", 18);
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));

        char hdr_info[64];
        snprintf(hdr_info, sizeof(hdr_info), "FPS: %.1f  |  Frame %lld", g_fps, (long long)g_frame_count);
        TextOutA(gdi_dc, 14, 24, hdr_info, (int)strlen(hdr_info));
    }

    // ── Paint all panels ───────────────────────────────────────────
    HFONT title_font = CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                    CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                    DEFAULT_PITCH, "Segoe UI");
    HFONT reg_font = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                  DEFAULT_PITCH, "Segoe UI");
    HFONT small_font = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                    CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                    DEFAULT_PITCH, "Segoe UI");

    for (int ri = 0; ri < g_region_count; ri++) {
        Region* r = &g_regions[ri];
        int rx = (int)r->x, ry = (int)r->y, rw = (int)r->w, rh = (int)r->h;

        if (r->type == 0) {
            // ── Panel background ───────────────────────────────────
            fill_rounded_rect(fb, stride, w, h, rx, ry, rw, rh, C_PANEL, 6);
            fill_rounded_rect(fb, stride, w, h, rx, ry, rw, rh, C_PANEL_BDR, 6);
            fill_rounded_rect(fb, stride, w, h, rx + 1, ry + 1, rw - 2, rh - 2, C_PANEL, 5);

            // Panel accent line at top
            fill_rect(fb, stride, rx + 14, ry + 24, 30, 2, C_ACCENT);

            if (gdi_dc && r->label) {
                SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
                SelectObject(gdi_dc, title_font);
                TextOutA(gdi_dc, rx + CONTENT_PAD, ry + 6, r->label, (int)strlen(r->label));
            }
        } else if (r->type == 1) {
            // ── Buttons ─────────────────────────────────────────────
            uint32_t btn_color;
            if (ri == g_highlight_btn) {
                btn_color = C_BUTTON_HL;
            } else if (r->id == 2) {
                btn_color = C_ACCENT4; // Exit
            } else if (r->id == 1 && g_toggle_state) {
                btn_color = C_ACCENT3; // Toggled color
            } else {
                btn_color = C_ACCENT2;
            }
            fill_rounded_rect(fb, stride, w, h, rx, ry, rw, rh, btn_color, 6);
            fill_rounded_rect(fb, stride, w, h, rx + 1, ry + 1, rw - 2, rh - 2,
                              ui_color_blend(0x40000000, btn_color), 5);

            if (gdi_dc && r->label) {
                SetTextColor(gdi_dc, RGB(0xFF, 0xFF, 0xFF));
                SetBkMode(gdi_dc, TRANSPARENT);
                SelectObject(gdi_dc, reg_font);

                char label[64];
                if (r->id == 0) {
                    snprintf(label, sizeof(label), "Click Me (%d)", g_click_count);
                } else {
                    snprintf(label, sizeof(label), "%s", r->label);
                }
                RECT br = { rx, ry, rx + rw, ry + rh };
                DrawTextA(gdi_dc, label, -1, &br, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        } else if (r->type == 2) {
            // ── Checkboxes ─────────────────────────────────────────
            int checked = 0;
            const char* label = "";
            if (r->id == 0) { checked = g_check_sound; label = "Sound"; }
            else if (r->id == 1) { checked = g_check_music; label = "Music"; }
            else if (r->id == 2) { checked = g_check_effects; label = "Effects"; }

            // Checkbox square
            int box_sz = 18;
            int box_x = rx;
            int box_y = ry + (rh - box_sz) / 2;
            fill_rounded_rect(fb, stride, w, h, box_x, box_y, box_sz, box_sz,
                              checked ? C_CHECK_ON : C_CHECK_OFF, 3);

            if (checked && gdi_dc) {
                // Draw checkmark
                SetTextColor(gdi_dc, RGB(0xFF, 0xFF, 0xFF));
                SelectObject(gdi_dc, reg_font);
                TextOutA(gdi_dc, box_x + 2, box_y - 1, "\xfb", 1); // checkmark
            }

            if (gdi_dc) {
                SetTextColor(gdi_dc, RGB(0xE8, 0xE8, 0xF0));
                SelectObject(gdi_dc, reg_font);
                TextOutA(gdi_dc, box_x + box_sz + 8, ry + (rh - 18) / 2,
                         label, (int)strlen(label));
            }
        } else if (r->type == 3) {
            // ── Slider labels and values ────────────────────────────
            if (r->id >= 10) {
                int* val = NULL;
                if (r->id == 10) val = &g_slider_volume;
                else if (r->id == 11) val = &g_slider_brightness;
                else val = &g_slider_speed;
                if (gdi_dc) {
                    SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
                    SelectObject(gdi_dc, small_font);
                    TextOutA(gdi_dc, rx, ry, r->label, (int)strlen(r->label));
                    // value
                    char vstr[16];
                    snprintf(vstr, 16, "%d%%", *val);
                    SetTextColor(gdi_dc, RGB(0xE8, 0xE8, 0xF0));
                    TextOutA(gdi_dc, rx + (int)r->w + 4, ry, vstr, (int)strlen(vstr));
                }
            } else {
                // ── Slider track ────────────────────────────────────
                int* val = NULL;
                uint32_t color = C_SLIDER_FG;
                if (r->id == 0) { val = &g_slider_volume; color = C_ACCENT; }
                else if (r->id == 1) { val = &g_slider_brightness; color = C_ACCENT3; }
                else { val = &g_slider_speed; color = C_ACCENT2; }

                // Background track
                int track_h = 6;
                int track_y = ry + (rh - track_h) / 2;
                fill_rounded_rect(fb, stride, w, h, rx, track_y, rw, track_h, C_SLIDER_BG, 3);

                // Filled portion
                int fill_w = (int)(rw * (*val) / 100.0);
                if (fill_w > 0) {
                    fill_rounded_rect(fb, stride, w, h, rx, track_y, fill_w, track_h, color, 3);
                }

                // Thumb
                int thumb_sz = 14;
                int thumb_x = rx + (int)(rw * (*val) / 100.0) - thumb_sz / 2;
                int thumb_y = ry + (rh - thumb_sz) / 2;
                fill_rounded_rect(fb, stride, w, h, thumb_x, thumb_y,
                                  thumb_sz, thumb_sz,
                                  ri == g_highlight_slider ? C_BUTTON_HL : C_SLIDER_TH,
                                  thumb_sz / 2);
            }
        } else if (r->type == 4) {
            // ── Text input area ─────────────────────────────────────
            if (r->id == 0) {
                // Input box
                fill_rounded_rect(fb, stride, w, h, rx, ry, rw, rh, C_INPUT_BG, 4);
                fill_rounded_rect(fb, stride, w, h, rx, ry, rw, rh,
                                  g_input_focused ? 0xFF21D4A1 : C_PANEL_BDR, 4);
                fill_rounded_rect(fb, stride, w, h, rx + 1, ry + 1,
                                  rw - 2, rh - 2, C_INPUT_BG, 3);

                if (gdi_dc) {
                    SetTextColor(gdi_dc, RGB(0xE8, 0xE8, 0xF0));
                    SetBkMode(gdi_dc, TRANSPARENT);
                    SelectObject(gdi_dc, reg_font);

                    // Input text
                    char display[68];
                    snprintf(display, sizeof(display), "%s", g_input_buf);
                    RECT ir = { rx + 8, ry + 2, rx + rw - 8, ry + rh - 2 };
                    DrawTextA(gdi_dc, display, -1, &ir, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                    // Cursor blink
                    if (g_input_focused && (g_cursor_blink % 30) < 20) {
                        int text_w = (int)strlen(g_input_buf) * 10;
                        int cx = rx + 10 + text_w;
                        if (cx < rx + rw - 4) {
                            fill_rect(fb, stride, cx, ry + 4, 2, rh - 8, 0xFFE8E8F0);
                        }
                    }

                    if (!g_input_focused) {
                        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
                        SelectObject(gdi_dc, small_font);
                        TextOutA(gdi_dc, rx + 8, ry + rh + 2,
                                 "Click to type. Type letters (auto-capitalize). Enter to submit.",
                                 62);
                    }
                }
            }
        } else if (r->type == 5) {
            // ── Progress bars ──────────────────────────────────────
            if (r->id < 100) {
                // Progress bar fill
                int idx = r->id;
                double pct = g_progress[idx] / 100.0;

                int bar_h = 12;
                int bar_y = ry + (rh - bar_h) / 2;
                fill_rounded_rect(fb, stride, w, h, rx, bar_y, rw, bar_h, C_SLIDER_BG, 4);

                // Animated fill
                int fill_w = (int)(rw * pct);
                if (fill_w > 0) {
                    uint32_t colors[] = {C_ACCENT, C_ACCENT2, C_ACCENT3, C_ACCENT4};
                    fill_rounded_rect(fb, stride, w, h, rx, bar_y,
                                      fill_w, bar_h, colors[idx], 4);
                }

                if (gdi_dc) {
                    SetTextColor(gdi_dc, RGB(0xE8, 0xE8, 0xF0));
                    SelectObject(gdi_dc, small_font);
                    char pct_str[8];
                    snprintf(pct_str, 8, "%d%%", (int)g_progress[idx]);
                    RECT pr = { rx, ry - 2, rx + rw, ry + rh + 2 };
                    DrawTextA(gdi_dc, pct_str, -1, &pr,
                              DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
                }

            } else {
                // File label
                if (gdi_dc && r->label) {
                    SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
                    SelectObject(gdi_dc, small_font);
                    TextOutA(gdi_dc, rx, ry, r->label, (int)strlen(r->label));
                }
            }
        }
    }

    DeleteObject(title_font);
    DeleteObject(reg_font);
    DeleteObject(small_font);

    // ── Status bar ─────────────────────────────────────────────────
    int sb_y = h - status_h;
    fill_rect(fb, stride, 0, sb_y, w, status_h, C_HEADER);
    fill_rect(fb, stride, 0, sb_y, w, 1, C_ACCENT);

    if (gdi_dc) {
        SetTextColor(gdi_dc, RGB(0x88, 0x88, 0xA0));
        SelectObject(gdi_dc, GetStockObject(DEFAULT_GUI_FONT));
        char status[256];
        snprintf(status, sizeof(status),
                 "Buttons: %d clicks  |  Sound:%s  Music:%s  Effects:%s  |  "
                 "Vol:%d%%  Bri:%d%%  Spd:%d%%  |  User: %s  |  %s  |  %.1f FPS",
                 g_click_count,
                 g_check_sound ? "ON" : "OFF",
                 g_check_music ? "ON" : "OFF",
                 g_check_effects ? "ON" : "OFF",
                 g_slider_volume, g_slider_brightness, g_slider_speed,
                 g_input_buf[0] ? g_input_buf : "(empty)",
                 g_active_widget, g_fps);
        TextOutA(gdi_dc, 10, sb_y + 5, status, (int)strlen(status));
    }
}

// ── Window subclass ────────────────────────────────────────────────────
static WNDPROC g_orig_wndproc = NULL;

static LRESULT CALLBACK demo_window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
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
            int type = 0;
            int idx = hit_test_region((double)mx, (double)my, &type);

            if (idx >= 0) {
                Region* r = &g_regions[idx];

                // ── Buttons (type 1) ───────────────────────────────
                if (type == 1) {
                    g_highlight_btn = idx;
                    if (r->id == 0) {
                        g_click_count++;
                        snprintf(g_active_widget, sizeof(g_active_widget),
                                 "Button clicked: %d times", g_click_count);
                    } else if (r->id == 1) {
                        g_toggle_state = !g_toggle_state;
                        snprintf(g_active_widget, sizeof(g_active_widget),
                                 "Color toggled: %s", g_toggle_state ? "ALT" : "NORMAL");
                    } else if (r->id == 2) {
                        PostQuitMessage(0);
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                }

                // ── Checkboxes (type 2) ────────────────────────────
                else if (type == 2) {
                    g_highlight_check = idx;
                    if (r->id == 0) {
                        g_check_sound = !g_check_sound;
                        snprintf(g_active_widget, sizeof(g_active_widget),
                                 "Sound: %s", g_check_sound ? "ON" : "OFF");
                    } else if (r->id == 1) {
                        g_check_music = !g_check_music;
                        snprintf(g_active_widget, sizeof(g_active_widget),
                                 "Music: %s", g_check_music ? "ON" : "OFF");
                    } else if (r->id == 2) {
                        g_check_effects = !g_check_effects;
                        snprintf(g_active_widget, sizeof(g_active_widget),
                                 "Effects: %s", g_check_effects ? "ON" : "OFF");
                    }
                    InvalidateRect(hwnd, NULL, FALSE);
                }

                // ── Sliders (type 3, draggable) ────────────────────
                else if (type == 3 && r->id < 10) {
                    g_dragging_slider = r->id;
                    g_highlight_slider = idx;
                    int v = slider_value_from_x(r, (double)mx);
                    if (r->id == 0) g_slider_volume = v;
                    else if (r->id == 1) g_slider_brightness = v;
                    else g_slider_speed = v;
                    snprintf(g_active_widget, sizeof(g_active_widget),
                             "Slider moved");
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, NULL, FALSE);
                }

                // ── Text input (type 4) ────────────────────────────
                else if (type == 4 && r->id == 0) {
                    g_input_focused = 1;
                    snprintf(g_active_widget, sizeof(g_active_widget),
                             "Text input focused");
                    InvalidateRect(hwnd, NULL, FALSE);
                }
            } else {
                g_input_focused = 0;
                g_highlight_btn = -1;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            g_highlight_btn = -1;
            g_dragging_slider = -1;
            g_highlight_slider = -1;
            ReleaseCapture();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (g_dragging_slider >= 0) {
                int mx = (int)(short)LOWORD(lp);
                // Find the slider region
                for (int i = 0; i < g_region_count; i++) {
                    Region* r = &g_regions[i];
                    if (r->type == 3 && r->id == g_dragging_slider) {
                        int v = slider_value_from_x(r, (double)mx);
                        if (r->id == 0) g_slider_volume = v;
                        else if (r->id == 1) g_slider_brightness = v;
                        else g_slider_speed = v;
                        InvalidateRect(hwnd, NULL, FALSE);
                        break;
                    }
                }
            }
            return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
        }
        case WM_CHAR: {
            // Text input handling
            if (g_input_focused) {
                char ch = (char)wp;
                if (ch == 8) { // Backspace
                    if (g_input_len > 0) {
                        g_input_buf[--g_input_len] = '\0';
                    }
                } else if (ch == 13) { // Enter
                    snprintf(g_active_widget, sizeof(g_active_widget),
                             "Submitted: %s", g_input_buf);
                    g_input_focused = 0;
                } else if (ch >= 32 && ch < 127) {
                    if (g_input_len < (int)sizeof(g_input_buf) - 2) {
                        g_input_buf[g_input_len++] = ch;
                        g_input_buf[g_input_len] = '\0';
                    }
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
        }
        case WM_KEYDOWN: {
            if (wp == VK_ESCAPE) {
                PostQuitMessage(0);
                return 0;
            }
            // Tab to focus input
            if (wp == VK_TAB) {
                g_input_focused = !g_input_focused;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            return 0;
        }
    }
    return CallWindowProcA(g_orig_wndproc, hwnd, msg, wp, lp);
}

// ── Main ───────────────────────────────────────────────────────────────
int main(void) {
    int win_w = 800, win_h = 600;

    printf("=== Widget System Demo — Kain Native UI ===\n");
    printf("Build: " __DATE__ " " __TIME__ "\n\n");
    printf("Panels: Buttons | Checkboxes | Sliders | Text Input | Progress\n");
    printf("Controls: Click buttons toggles. Drag sliders. Type in input. Tab focuses.\n\n");

    // Init
    snprintf(g_active_widget, sizeof(g_active_widget), "Ready");

    abi_ui_reset();
    int64_t session = abi_ui_session_create("WidgetDemo", win_w, win_h);
    if (session <= 0) { fprintf(stderr, "FAIL: session_create\n"); return 1; }

    abi_ui_window_open(session, "Widget System Demo — Kain Native UI", win_w, win_h);
    if (abi_ui_host_attach(session, "winit") != 0) {
        fprintf(stderr, "FAIL: host_attach\n"); return 1;
    }
    printf("Session: %lld  Backend: %s\n", (long long)session, abi_ui_host_backend(session));

    KainNativeUiSession* ks = abi_ui_find_session(session);
    if (!ks || !ks->host_state) { fprintf(stderr, "FAIL: no host state\n"); return 1; }
    KainWin32UiHost* host = (KainWin32UiHost*)ks->host_state;

    // Subclass
    g_orig_wndproc = (WNDPROC)SetWindowLongPtrA(host->hwnd, GWLP_WNDPROC,
                                                  (LONG_PTR)demo_window_proc);
    printf("Window: hwnd=%p  fb=%p  %dx%d\n",
           (void*)host->hwnd, (void*)host->framebuffer, host->width, host->height);

    // Build node tree
    int64_t root = abi_ui_node_create(session, "root");
    abi_ui_node_set_rect(session, root, 0, 0, win_w, win_h);
    int64_t bg = abi_ui_node_create(session, "bg");
    abi_ui_node_set_parent(session, bg, root);
    abi_ui_node_set_rect(session, bg, 0, 0, win_w, win_h);
    abi_ui_node_set_style_string(session, bg, "fill_color", "#0F172A");

    // Layout panels
    layout_panels(win_w, win_h);

    printf("\nFrame loop running. %d regions registered.\n", g_region_count);
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

        // Update FPS
        g_frame_count++;
        g_fps_frames++;
        g_fps_timer += 16.67;
        if (g_fps_timer >= 1000.0) {
            g_fps = (double)g_fps_frames * 1000.0 / g_fps_timer;
            g_fps_timer = 0.0;
            g_fps_frames = 0;
        }

        // Update progress bars
        for (int i = 0; i < 4; i++) {
            g_progress[i] += g_prog_speed[i];
            if (g_progress[i] >= 100.0) {
                g_progress[i] = 0.0;
            }
        }

        // Update cursor blink
        g_cursor_blink++;

        // Render
        if (host->framebuffer) {
            paint_demo((uint32_t*)host->framebuffer,
                      host->width, host->height, host->fb_stride / 4,
                      host->hdc_buffer);
            InvalidateRect(host->hwnd, NULL, FALSE);
        }

        if (frame % 60 == 0) {
            printf("Frame %lld | FPS: %.1f | Clicks: %d | Sound:%s Music:%s Effects:%s | "
                   "Vol:%d Bri:%d Spd:%d | Prog:[%.0f %.0f %.0f %.0f] | User:%s\n",
                   (long long)frame, g_fps, g_click_count,
                   g_check_sound ? "ON" : "OFF",
                   g_check_music ? "ON" : "OFF",
                   g_check_effects ? "ON" : "OFF",
                   g_slider_volume, g_slider_brightness, g_slider_speed,
                   g_progress[0], g_progress[1], g_progress[2], g_progress[3],
                   g_input_buf);
        }

        frame++;
        Sleep(16);
    }

    printf("\nShutdown after %lld frames.\n", (long long)frame);
    printf("Final state: Clicked %d times | User: %s\n", g_click_count, g_input_buf);
    abi_ui_session_destroy(session);
    printf("Done.\n");
    return 0;
}
