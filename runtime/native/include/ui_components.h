// ============================================================================
//  ui_components.h — High-Level UI Component Primitives
//  ============================================================================
//  Immediate-mode component library built on top of the Kain rendering
//  substrate (kain_render_software.h). Each component is a single function
//  that draws, handles interaction, and returns meaningful results.
//
//  Design principles:
//    1. No global state — UiComponentsContext holds everything
//    2. No malloc in hot path — all temporaries on stack
//    3. DPI-aware — all sizes multiplied by ctx->dpi_scale
//    4. Data-driven colors — all colors come from UiTheme* (ui_theme.h).
//       Set ctx.theme before calling components. See ui_theme_dark().
//    5. Components are composable — each returns a bounds rect
//    6. Interaction query pattern (inspired by Clay):
//       query during rendering, not through callbacks
//    7. No retained-mode node tree dependency
//       (uses KainSoftwareRenderer* directly)
//    8. No Win32 dependency — mouse state supplied via context
//
//  Usage:
//    UiComponentsContext ctx = {
//        .renderer = my_renderer,
//        .session_id = session_id,
//        .default_font_id = font_id,
//        .dpi_scale = 1.0f,
//        .mouse_x = mouse_x, .mouse_y = mouse_y,
//        .mouse_down = mouse_down,
//    };
//
//    UiButtonResult btn = ui_button(&ctx, 10, 10, 120, 32, "Click Me");
//    if (btn.clicked) { /* handle click */ }
//
//    float slider_val = 0.5f;
//    UiSliderResult sl = ui_slider(&ctx, 10, 50, 200, 20, &slider_val);
//    if (sl.changed) { /* use sl.value */ }
// ============================================================================

#ifndef KAIN_UI_COMPONENTS_H
#define KAIN_UI_COMPONENTS_H

#include <stdint.h>
#include <stdbool.h>
#include "kain_geometry.h"
#include "ui_theme.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Forward declaration ───────────────────────────────────────────────
struct KainSoftwareRenderer;

// ── Context (holds session, font, renderer, theme, input state) ─────
// All components receive a pointer to this context. Multiple contexts
// can exist independently — no global state.
// Colors are read from theme (ui_theme.h) — set ctx.theme before calling components.
typedef struct UiComponentsContext {
    struct KainSoftwareRenderer* renderer;   // Must be set before calling components
    int64_t   session_id;                     // UI session for font resource lookups
    int64_t   default_font_id;               // Font resource ID for text (0 = no text)
    float     dpi_scale;                      // DPI multiplier (1.0 = 100%, 2.0 = 200%)
    float     mouse_x, mouse_y;              // Current mouse position in logical pixels
    bool      mouse_down;                     // Is left mouse button held this frame?
    float     frame_time;                     // Delta time in seconds (for animation)
    const struct UiTheme* theme;             // Color theme (set to ui_theme_dark() by default)
} UiComponentsContext;

// ── Component return values ──────────────────────────────────────────
// Each component returns a typed result with interaction flags and
// the bounding box for layout queries.

// Button result: clicked is true ONLY on the frame the button is pressed.
typedef struct UiButtonResult {
    bool      clicked;    // true on press-down frame
    bool      hovered;    // true while mouse is inside bounds
    kainRect  bounds;     // bounding box in logical pixels
} UiButtonResult;

// Slider result: changed is true when value was modified by drag.
typedef struct UiSliderResult {
    bool      changed;    // true if value changed this frame
    float     value;      // current value [0..1]
    kainRect  bounds;
} UiSliderResult;

// Label result: bounds for layout/compositor.
typedef struct UiLabelResult {
    kainRect  bounds;
} UiLabelResult;

// Panel result: open is false if close button was clicked.
typedef struct UiPanelResult {
    bool      open;       // false if close button was clicked
    kainRect  title_bounds; // title bar area (for drag handling)
    kainRect  content_bounds; // content area (for child placement)
} UiPanelResult;

// Checkbox result: toggled is true on the frame the box was clicked.
typedef struct UiCheckboxResult {
    bool      toggled;    // true on the frame value changed
    bool      value;      // current toggle state
    kainRect  bounds;
} UiCheckboxResult;

// Progress bar: purely visual, no interaction.
typedef struct UiProgressResult {
    kainRect  bounds;
} UiProgressResult;

// ── Component functions ──────────────────────────────────────────────

// Draw a clickable button. Returns clicked=true on the frame the mouse
// presses down inside the button bounds.
// `x`, `y`, `width`, `height`: position in the parent coordinate space.
// `label`: text shown on the button (can be NULL for icon-only).
UiButtonResult ui_button(UiComponentsContext* ctx, float x, float y,
                          float width, float height, const char* label);

// Draw static text at the given position. Returns bounding box.
// The label's natural width is used; the caller provides the desired
// width/height (or use the returned bounds for exact sizing).
UiLabelResult ui_label(UiComponentsContext* ctx, float x, float y,
                        float width, float height, const char* text,
                        float font_size);

// Draw a horizontal slider. `value` is an in/out parameter in [0..1].
// Returns changed=true when the user drags the thumb.
UiSliderResult ui_slider(UiComponentsContext* ctx, float x, float y,
                          float width, float height, float* value);

// Draw a titled panel with optional close button.
// If `open` is non-NULL, a close button (X) appears and sets *open=false
// on click. The caller should check result.open or *open to skip drawing
// children when the panel is closing.
// Use result.content_bounds to position children inside the panel.
UiPanelResult ui_panel(UiComponentsContext* ctx, float x, float y,
                        float width, float height, const char* title,
                        bool* open);

// Draw a toggle-able checkbox with a label.
// `value` is an in/out pointer to the boolean state.
// Returns toggled=true on the click frame.
UiCheckboxResult ui_checkbox(UiComponentsContext* ctx, float x, float y,
                              float width, float height, const char* label,
                              float font_size, bool* value);

// Draw a progress bar showing a ratio [0..1].
// `value` is clamped to [0..1].
UiProgressResult ui_progress(UiComponentsContext* ctx, float x, float y,
                              float width, float height, float value);

// ── Layout helpers ──────────────────────────────────────────────────
// Simple row/column placement helpers for basic layout needs.
// These are NOT a full layout engine — they just advance a cursor
// and return the next position.

// Arrange N children in a row. Returns the x position after the last child.
// `count`: number of children.
// `widths`: array of `count` widths for each child.
// `gap`: horizontal spacing between children.
float ui_layout_row(float start_x, float y, int count,
                     const float* widths, float gap);

// Arrange N children in a column. Returns the y position after the last child.
// `heights`: array of `count` heights for each child.
// `gap`: vertical spacing between children.
float ui_layout_column(float x, float start_y, int count,
                        const float* heights, float gap);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_UI_COMPONENTS_H */
