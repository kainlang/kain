// ============================================================================
//  ui_widget.h — Kain Native UI Widget Library
//  ============================================================================
//  Immediate-mode widget API over the Kain retained-mode UI system.
//  Inspired by microui (rxi), built on the Kain ABI (ui_system.h).
//
//  Each widget is a single function that:
//    1. Creates/reuses nodes through the ABI (retained-mode)
//    2. Draws directly into the DIB framebuffer
//    3. Handles hover/click/focus state automatically
//    4. Returns meaningful data (clicked, toggled, changed)
//    5. Advances the layout cursor
//
//  Usage:
//    KainUiWidgetContext* ctx = ui_widget_create(session_id);
//    while (running) {
//        ui_widget_begin_frame(ctx);
//        ui_panel(ctx, "Controls", 10, 10, 300, 400);
//            if (ui_button(ctx, "Click")) { ... }
//            ui_checkbox(ctx, "Enable", &flag);
//            ui_slider(ctx, &val, 0, 100);
//        ui_panel_end(ctx);
//        ui_widget_end_frame(ctx);
//        // ... host present + message pump ...
//    }
// ============================================================================

#ifndef UI_WIDGET_H
#define UI_WIDGET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Color constants (0xAARRGGBB) ──────────────────────────────────────
#define UI_COLOR_BG        0xFF1A1A24
#define UI_COLOR_SURFACE   0xFF252540
#define UI_COLOR_SURFACE2  0xFF2E2E48
#define UI_COLOR_HEADER    0xFF1E1E32
#define UI_COLOR_ACCENT    0xFF21D4A1
#define UI_COLOR_ACCENT2   0xFF4A90D9
#define UI_COLOR_ACCENT3   0xFFE8914A
#define UI_COLOR_ACCENT4   0xFFE84A5F
#define UI_COLOR_TEXT      0xFFE8E8F0
#define UI_COLOR_TEXT_DIM  0xFF8888A0
#define UI_COLOR_BORDER    0xFF3A3A5C
#define UI_COLOR_BUTTON    0xFF303050
#define UI_COLOR_BUTTON_HL 0xFF404068
#define UI_COLOR_BUTTON_PR 0xFF505080
#define UI_COLOR_TITLE_BG  0xFF23233C
#define UI_COLOR_INPUT_BG  0xFF0A0A14
#define UI_COLOR_SLIDER    0xFF3A3A5C
#define UI_COLOR_SLIDER_F  0xFF2A2A44

// ── Default sizes ─────────────────────────────────────────────────────
#define UI_BUTTON_WIDTH   100
#define UI_BUTTON_HEIGHT  30
#define UI_CHECKBOX_SIZE  18
#define UI_SLIDER_WIDTH   200
#define UI_SLIDER_HEIGHT  20
#define UI_TEXTBOX_WIDTH  160
#define UI_TEXTBOX_HEIGHT 26
#define UI_PROGRESS_WIDTH 150
#define UI_PROGRESS_HEIGHT 18
#define UI_LABEL_HEIGHT   20
#define UI_PADDING        8
#define UI_SPACING        4

// ── Maximum layout items per row/column ───────────────────────────────
#define UI_MAX_LAYOUT_ITEMS 16
#define UI_MAX_CONTAINERS   16
#define UI_WIDGET_KEY_SIZE  48

// ── Font system ───────────────────────────────────────────────────────
#define UI_WIDGET_MAX_FONTS 8

// Entry in the font table: tracks the resource ID and the TTF data we
// keep alive for the session. ttf_data is freed in ui_widget_destroy().
typedef struct UiFontEntry {
    int64_t font_id;       // font resource ID from abi_ui_font_load_ttf()
    uint8_t* ttf_data;     // raw TTF file bytes (kept alive for session)
    int ttf_len;           // length of TTF data in bytes
} UiFontEntry;

// ── Forward declarations ──────────────────────────────────────────────
struct KainWin32UiHost;
struct KainNativeUiSession;

// ── Container stack entry ─────────────────────────────────────────────
typedef struct UiContainer {
    int64_t node_id;
    double x, y, w, h;
    double cursor_x, cursor_y;
} UiContainer;

// ── Widget Context ────────────────────────────────────────────────────
typedef struct KainUiWidgetContext {
    // Session ID passed at creation
    int64_t session_id;

    // DPI scale factor (e.g., 2.0 on a 4K display at 200%)
    double dpi_scale;

    // Cached host pointer (for framebuffer + GDI access)
    struct KainWin32UiHost* host;
    struct KainNativeUiSession* session;

    // Layout state
    double layout_x, layout_y;
    double layout_next_y;
    int layout_type;          // 0=row, 1=column
    int layout_item;          // current item index in row/col
    int layout_count;         // total items in current row/col
    int layout_sizes[UI_MAX_LAYOUT_ITEMS];
    int layout_size_count;

    // Widget ID counter (per frame, for stable keys)
    int widget_counter;

    // Mouse state (updated each frame via Win32)
    double mouse_x, mouse_y;
    int mouse_down;
    int mouse_down_prev;

    // Interaction tracking across widgets (by node_id)
    int64_t pressed_node;     // set on press, cleared by matching widget on release
    int64_t last_pressed;     // saved from pressed_node before clearing in begin_frame
    int64_t hovered_node;
    int64_t focused_node;

    // Text editing state (for textbox)
    char edit_buf[256];
    int edit_len;
    int edit_cursor;
    int edit_active;          // 1 if edit_buf has pending non-committed changes
    int edit_changed;         // 1 if textbox content changed this frame

    // Container stack for panel nesting
    UiContainer container_stack[UI_MAX_CONTAINERS];
    int container_depth;

    // Font table — loaded via ui_widget_load_font() / ui_widget_load_default_font()
    UiFontEntry fonts[UI_WIDGET_MAX_FONTS];   // loaded font entries
    int font_count;                           // number of loaded fonts
    int default_font;                         // index into fonts[] for default, -1 if none

    // Colors
    uint32_t color_bg;
    uint32_t color_surface;
    uint32_t color_surface2;
    uint32_t color_header;
    uint32_t color_accent;
    uint32_t color_accent2;
    uint32_t color_text;
    uint32_t color_text_dim;
    uint32_t color_border;
    uint32_t color_button;
    uint32_t color_button_hover;
    uint32_t color_button_pressed;
    uint32_t color_title_bg;
    uint32_t color_input_bg;
} KainUiWidgetContext;

// ── Lifecycle ─────────────────────────────────────────────────────────
KainUiWidgetContext* ui_widget_create(int64_t session_id);
void ui_widget_destroy(KainUiWidgetContext* ctx);

// ── Frame lifecycle ───────────────────────────────────────────────────
// Call at the start of each frame (updates mouse, resets widget counter)
void ui_widget_begin_frame(KainUiWidgetContext* ctx);

// Call after all widgets are drawn (flushes any pending state)
void ui_widget_end_frame(KainUiWidgetContext* ctx);

// ── Layout ────────────────────────────────────────────────────────────
// Set up a row of [count] columns with specified widths (in pixels).
// Each widget call auto-advances to the next column.
void ui_layout_row(KainUiWidgetContext* ctx, int count, const int* widths);

// Set up a column of [count] rows with specified heights.
void ui_layout_column(KainUiWidgetContext* ctx, int count, const int* heights);

// Set a specific size for the next widget (width, height in pixels).
void ui_layout_set_next(KainUiWidgetContext* ctx, int width, int height);

// ── Widgets ───────────────────────────────────────────────────────────

// Button: clickable with hover/press states.
// Returns 1 if the button was clicked (press + release on same widget).
int ui_button(KainUiWidgetContext* ctx, const char* label);

// Label: static text display.
// Returns the node_id for reference.
int64_t ui_label(KainUiWidgetContext* ctx, const char* text);

// Checkbox: togglable square + label.
// *value is toggled on click. Returns 1 if toggled.
int ui_checkbox(KainUiWidgetContext* ctx, const char* label, int* value);

// Slider: horizontal track with draggable thumb.
// *value is clamped to [lo, hi]. Returns 1 if changed.
int ui_slider(KainUiWidgetContext* ctx, double* value, double lo, double hi);

// Textbox: single-line text input with cursor.
// buf must have buf_size bytes. Returns 1 if content changed.
int ui_textbox(KainUiWidgetContext* ctx, char* buf, int buf_size);

// Panel: titled container with content area.
// Returns the panel node_id for reference. Call ui_panel_end() to close.
int64_t ui_panel(KainUiWidgetContext* ctx, const char* title, double x, double y, double w, double h);
void ui_panel_end(KainUiWidgetContext* ctx);

// Progress bar: visual progress indicator.
// Shows value/max progress. Returns node_id.
int64_t ui_progress(KainUiWidgetContext* ctx, const char* label, double value, double max);

// Window: draggable, closable window container.
// *x, *y are the window position (drag updates them). *open is 0 when closed.
// Returns 1 while the window should stay open.
int ui_window(KainUiWidgetContext* ctx, const char* title, double* x, double* y,
              double w, double h, int* open);

// ── Low-level drawing helpers (exposed for custom widgets) ────────────

// Fill a rectangle in the framebuffer with bounds checking.
void ui_widget_fill_rect(uint32_t* fb, int stride, int fb_w, int fb_h,
                         int x, int y, int w, int h, uint32_t color);

// Fill a rounded rectangle.
void ui_widget_fill_rounded_rect(uint32_t* fb, int stride, int fb_w, int fb_h,
                                 int x, int y, int w, int h, uint32_t color, int r);

// Draw text at (x,y) using stb_truetype glyph rasterization.
// The font loaded in ctx->default_font_id is used.
// size parameter is kept for API compatibility but the loaded font size is used.
void ui_widget_draw_text(KainUiWidgetContext* ctx, int x, int y,
                         const char* text, uint32_t color, int size);

// Draw text with a specific font resource ID (use 0 for default font).
void ui_widget_draw_text_ex(KainUiWidgetContext* ctx, int x, int y,
                            const char* text, uint32_t color, int size,
                            int64_t font_id);

// Draw text centered in a rect using stb_truetype glyph rasterization.
void ui_widget_draw_text_centered(KainUiWidgetContext* ctx,
                                  int x, int y, int w, int h,
                                  const char* text, uint32_t color, int size);

// Measure text width in pixels (using stb_truetype metrics).
int ui_widget_text_width(KainUiWidgetContext* ctx, const char* text);

// ── Font loading (cross-platform, no GDI) ────────────────────────────

// Load a font from a specific .ttf file path.
// Returns the font resource ID (> 0) on success, or 0 on failure.
// The loaded font is added to ctx->fonts[] and can be used for text rendering.
int64_t ui_widget_load_font(KainUiWidgetContext* ctx, const char* filepath, double size);

// Load the default system font. Searches platform-specific paths:
//   Windows: C:/Windows/Fonts/segoeui.ttf → arial.ttf → tahoma.ttf
//   Linux:   /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
//   macOS:   /System/Library/Fonts/Helvetica.ttc → SFNS.ttf
// The first successfully loaded font becomes the default.
// Returns the font resource ID (> 0) on success, or 0 if no font found.
int64_t ui_widget_load_default_font(KainUiWidgetContext* ctx, double size);

#ifdef __cplusplus
}
#endif

#endif /* UI_WIDGET_H */
