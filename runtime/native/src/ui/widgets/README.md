# Kain Native UI — Widget Library

An **immediate-mode widget library** for the Kain retained-mode UI system. Inspired by [microui](https://github.com/rxi/microui), built on the Kain ABI (`ui_system.h`).

**Lines of code:** ~1,450 (ui_widget.h + ui_widget.c)  
**Dependencies:** Kain UI system (`ui_system.c`, `ui_host_adapter.c`, `ui_renderer.c`, `ui_layout.c`, `ui_color.c`, `input_system.c`)  
**Platform:** Windows (Win32 GDI DIB framebuffer)

---

## What It Provides

8 interactive widgets, a layout system, and a per-frame context that handles mouse tracking, click detection, focus management, and text rendering:

| Widget | Returns | State |
|--------|---------|-------|
| `ui_button` | 1 if clicked | Normal → Hover → Pressed (color change) |
| `ui_label` | node_id | Static text display |
| `ui_checkbox` | 1 if toggled | Checked / Unchecked with ✓ mark |
| `ui_slider` | 1 if value changed | Drag-to-adjust with thumb |
| `ui_textbox` | 1 if content changed | Focus + cursor + keyboard input |
| `ui_panel` | node_id | Titled container with content area |
| `ui_progress` | node_id | Animated bar with percentage text |
| `ui_window` | 1 if still open | Draggable, closable with × button |

---

## Architecture

```
Application (test_widgets.c)
  └── KainUiWidgetContext          ← per-frame state, mouse, layout
        ├── ui_button()            ← draws, interacts, returns click
        ├── ui_checkbox()          ← draws, toggles value
        ├── ui_slider()            ← draws, updates value on drag
        ├── ui_textbox()           ← draws, handles keyboard
        ├── ui_panel/panel_end()   ← container with title bar
        ├── ui_progress()          ← shows value/max ratio
        ├── ui_window()            ← draggable window with close
        └── ui_label()             ← static text
  └── ABI calls (ui_system.h)
        ├── abi_ui_node_*          ← retained-mode node tree
        ├── abi_ui_begin/end_frame ← frame lifecycle
        └── abi_ui_find_session    ← host state access
  └── Win32 GDI
        ├── TextOutA / DrawTextA   ← text rendering
        └── Direct pixel writes    ← rects, borders, checkmarks
```

### Rendering Pipeline

Each frame follows this sequence:

1. **`ui_widget_begin_frame(ctx)`** — Updates mouse position + button state from Win32, resets widget counter and layout
2. **Widget calls** — Each widget:
   - Gets its position from the layout system
   - Creates/finds nodes via the ABI (retained-mode)
   - Checks mouse interaction (hover, press, click, drag)
   - Draws directly into the DIB framebuffer (pixel writes + GDI text)
   - Advances the layout cursor
3. **`ui_widget_end_frame(ctx)`** — Cleans up stale pressed state
4. **`InvalidateRect(hwnd, NULL, FALSE)`** — Triggers WM_PAINT → BitBlt → screen

### Interaction Model

The widget context tracks raw mouse state (`mouse_x`, `mouse_y`, `mouse_down`, `mouse_down_prev`). Each widget manages its own press/click/drag via:

- **Press:** `if (mouse_down && !mouse_down_prev && hovered) → ctx->pressed_node = nid`
- **Click:** `if (!mouse_down && mouse_down_prev && ctx->pressed_node == nid && hovered) → click!`
- **Drag:** `if (mouse_down && ctx->pressed_node == nid) → update value from mouse position`
- **Release cleanup:** `end_frame` clears `pressed_node` if mouse was released but no widget matched

---

## Quick Start

### Build

```batch
cd X:\runtime\native\src\ui\widgets
build.bat
```

This compiles `test_widgets.exe` with clang (or MSVC fallback).

### Run

```batch
test_widgets.exe
```

**Controls:**
- Hover over buttons/sliders to see hover state
- Click buttons → increments counter
- Check/uncheck checkboxes → toggles state
- Drag slider thumb → adjusts value
- Click textbox, type characters → text input
- Drag window by its title bar → moves window
- Click × on window → closes it
- Press **Esc** → exits
- Click "Show Window" → reopens closed window

---

## API Reference

### Lifecycle

```c
KainUiWidgetContext* ui_widget_create(int64_t session_id);
void ui_widget_destroy(KainUiWidgetContext* ctx);

void ui_widget_begin_frame(KainUiWidgetContext* ctx);
void ui_widget_end_frame(KainUiWidgetContext* ctx);
```

- `create` — Must be called after `abi_ui_host_attach()` so the host pointer is available
- `begin_frame` — Updates mouse state, resets layout and widget counter. Call once per frame before any widget.
- `end_frame` — Cleans up stale interaction state. Call once per frame after all widgets.

### Layout

```c
void ui_layout_row(KainUiWidgetContext* ctx, int count, const int* widths);
void ui_layout_column(KainUiWidgetContext* ctx, int count, const int* heights);
void ui_layout_set_next(KainUiWidgetContext* ctx, int width, int height);
```

- `layout_row` — Next N widgets are placed horizontally with specified column widths
- `layout_column` — Next N widgets are placed vertically with specified row heights
- `layout_set_next` — Single widget override for width/height
- **Inside a panel**, layouts are automatically constrained to the panel's content area
- **Without a panel**, widgets auto-position from (0,0) and advance horizontally, wrapping
- **Widget defaults:** Button (100×30), Slider (200×20), Textbox (160×26), Progress (150×18), Checkbox (auto), Label (auto)

### Widgets

#### `int ui_button(ctx, label)`

Creates a clickable button with hover/press visual states.

| State | Fill Color |
|-------|-----------|
| Normal | `#303050` |
| Hover | `#404068` |
| Pressed | `#505080` |

**Returns:** 1 on click (press + release on same widget), 0 otherwise.

---

#### `int64_t ui_label(ctx, text)`

Static text display. Auto-sizes to text width.

**Returns:** node_id of the label.

---

#### `int ui_checkbox(ctx, label, *value)`

Toggleable checkbox with visual ✓ indicator.

- Checked: accent fill + white checkmark
- Unchecked: dark fill + thin border
- Click toggles `*value` between 0 and 1

**Returns:** 1 if toggled this frame.

---

#### `int ui_slider(ctx, *value, lo, hi)`

Horizontal slider with draggable thumb.

- Track: 200×6px, dark
- Filled portion: accent color
- Thumb: 10×18px, draggable
- `*value` is clamped to [lo, hi]
- Click-drag updates `*value` continuously

**Returns:** 1 if value changed this frame.

---

#### `int ui_textbox(ctx, buf, buf_size)`

Single-line text input field.

- Cursor shown when focused
- Click to focus, click elsewhere to lose focus
- Keyboard input: letters, digits, space, backspace
- Flashing cursor at text extent

**Returns:** 1 if content changed this frame.

---

#### `int64_t ui_panel(ctx, title, x, y, w, h)`

Titled container panel. All subsequent widget calls are parented to the panel's content area until `ui_panel_end()`.

- Title bar: 28px tall with accent underline
- Content area: starts at (x+8, y+30+8)
- Layout auto-wraps within content width

**Returns:** node_id. Close with `ui_panel_end(ctx)`.

---

#### `void ui_panel_end(ctx)`

Closes the most recent panel. Restores layout position to parent container.

---

#### `int64_t ui_progress(ctx, label, value, max)`

Progress bar with percentage text.

- Background: dark rounded rect
- Fill: accent color, width = ratio × bar width
- Text: "XX%" centered in bar
- Optional label to the right of the bar

**Returns:** node_id.

---

#### `int ui_window(ctx, title, *x, *y, w, h, *open)`

Draggable, closable window container.

- Title bar dragging: click-drag on title bar moves window, updates `*x`/`*y`
- Close button (×) in top-right corner: sets `*open = 0`
- Shadow under window
- All widget calls between `ui_window()` and `ui_panel_end()` are parented to the window

**Returns:** 1 while window should stay open (content of `*open`).

### Colors

The widget context comes pre-configured with a dark theme. You can override any color:

```c
ctx->color_accent   = 0xFF00FF00;  // bright green accent
ctx->color_button   = 0xFF333355;  // custom button color
ctx->color_text     = 0xFFFFFFFF;  // white text
```

Built-in color constants:

| Constant | Value | Usage |
|----------|-------|-------|
| `UI_COLOR_BG` | `0xFF1A1A24` | Window background |
| `UI_COLOR_SURFACE` | `0xFF252540` | Panel/card background |
| `UI_COLOR_SURFACE2` | `0xFF2E2E48` | Elevated surface |
| `UI_COLOR_HEADER` | `0xFF1E1E32` | Header/title bar |
| `UI_COLOR_ACCENT` | `0xFF21D4A1` | Primary accent (green) |
| `UI_COLOR_ACCENT2` | `0xFF4A90D9` | Secondary accent (blue) |
| `UI_COLOR_TEXT` | `0xFFE8E8F0` | Primary text |
| `UI_COLOR_TEXT_DIM` | `0xFF8888A0` | Dim/muted text |
| `UI_COLOR_BORDER` | `0xFF3A3A5C` | Borders |
| `UI_COLOR_BUTTON` | `0xFF303050` | Button (normal) |
| `UI_COLOR_BUTTON_HL` | `0xFF404068` | Button (hover) |
| `UI_COLOR_BUTTON_PR` | `0xFF505080` | Button (pressed) |

---

## Example: Full Widget Demo

```c
#include "ui_widget.h"
#include "ui_system.h"
#include "ui_system_internal.h"

// ... session setup ...

KainUiWidgetContext* ctx = ui_widget_create(session);

while (running) {
    // Pump messages
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { ... }
    
    // Begin frame
    abi_ui_begin_frame(session, 16.67);
    ui_widget_begin_frame(ctx);
    
    // Clear framebuffer to dark background
    clear_framebuffer((uint32_t*)host->framebuffer, host->width, host->height,
                      host->fb_stride / 4, UI_COLOR_BG);
    
    // Layout widgets
    ui_panel(ctx, "Controls", 10, 10, 300, 200);
    {
        if (ui_button(ctx, "Click")) click_count++;
        ui_checkbox(ctx, "Enable", &flag);
        ui_slider(ctx, &value, 0, 100);
        ui_textbox(ctx, buf, sizeof(buf));
        ui_progress(ctx, "Progress", progress, 100);
    }
    ui_panel_end(ctx);
    
    // End frame
    ui_widget_end_frame(ctx);
    abi_ui_end_frame(session);
    
    // Trigger display
    InvalidateRect(host->hwnd, NULL, FALSE);
    Sleep(16);
}
```

---

## Drawing Helpers

The widget library exposes pixel-level drawing helpers for custom widgets:

```c
void ui_widget_fill_rect(uint32_t* fb, int stride, int fb_w, int fb_h,
                         int x, int y, int w, int h, uint32_t color);

void ui_widget_fill_rounded_rect(uint32_t* fb, int stride, int fb_w, int fb_h,
                                 int x, int y, int w, int h, uint32_t color, int r);

void ui_widget_draw_text(struct KainWin32UiHost* host, int x, int y,
                         const char* text, uint32_t color, int size);

void ui_widget_draw_text_centered(struct KainWin32UiHost* host,
                                  int x, int y, int w, int h,
                                  const char* text, uint32_t color, int size);

int ui_widget_text_width(struct KainWin32UiHost* host, const char* text);
```

All pixel coordinates are bounds-checked against framebuffer dimensions.

---

## File Map

```
widgets/
├── ui_widget.h           — Public API header (all widget declarations)
├── ui_widget.c           — Implementation (~850 lines)
├── test_widgets.c        — Interactive demo program
├── stubs.c               — Link stubs (string_new, component_surface)
├── build.bat             — Build script
├── README.md             — This file
└── oracle_vision.png     — Screenshot of the running demo
```

---

## Dependencies

The widget library links against these C files from the Kain UI system:

| File | Path | Provides |
|------|------|----------|
| `ui_system.c` | `../ui_system.c` | Session/node/style/event ABI |
| `ui_host_adapter.c` | `../ui_host_adapter.c` | Win32 window + DIB framebuffer |
| `ui_renderer.c` | `../ui_renderer.c` | Node tree → pixel renderer |
| `ui_layout.c` | `../ui_layout.c` | Flexbox-style layout engine |
| `ui_color.c` | `../ui_color.c` | Color parsing (#hex, rgba, named) |
| `input_system.c` | `../../core/input_system.c` | Universal input event bridge |
| `stubs.c` | `stubs.c` | `string_new`, `kain_component_surface_resolve` |

Include paths:
```
-I../../../include    (ui_system.h, ui_renderer.h, ui_layout.h, ui_color.h, etc.)
-I..                  (ui_system_internal.h, ui_host_adapter.h)
-I../../core          (input_system.h)
```

---

## Limitations

- **Win32-only** — Uses Win32 GDI for text rendering. A future port could use Direct2D or a software font rasterizer.
- **Single textbox** — Keyboard input uses `GetAsyncKeyState` polling. Proper IME support requires `WM_CHAR` via window subclassing.
- **No scrollbars** — Panels don't scroll if content overflows.
- **No tree node** — Not implemented yet.
- **DPI-aware** — The session auto-syncs to the actual DPI-scaled client rect, so widget positions match display pixels.
- **Font** — Uses "Segoe UI" with CreateFontA. Falls back to DEFAULT_GUI_FONT. Font handle is created/destroyed per text draw (simplified for now).

---

## Future Work

- [ ] Port to use node tree rendering (ui_render_frame) instead of direct framebuffer
- [ ] Add `ui_treenode()`, `ui_header()`, `ui_popup()` to match microui's full widget set
- [ ] Add scrollbar support to panels
- [ ] Use `abi_ui_push_event`/`abi_ui_poll_event` for input instead of Win32 direct API
- [ ] Support the non-Winit backends (Vulkan, D3D12) via software fallback
- [ ] Expose widget state (hovered/pressed/focused) for custom styling
