# Kain UI – Complete Guide

**The single source of truth for writing UI in Kain.** From hardware framebuffer to high-level components, this document covers every layer of the Kain UI stack.

---

## 1. Architecture Overview

Kain's UI system is a **4-layer stack** built on a retained-mode C11 engine wrapped by a Kain stdlib and consumable through idiomatic Kain components, widgets, or raw ABI calls.

```
 ┌───────────────────────────────────────────────────────────┐
 │  LAYER 3: Kain Components & Widgets                       │
 │  component MyApp ... <panel><button onClick={...}>        │
 │  widget::button(ctx, "Click")  widget::slider(...)        │
 │  std::ui, std::ui::widget, std::ui::component, std::input │
 ├───────────────────────────────────────────────────────────┤
 │  LAYER 2: ABI Bridge (90+ functions)                      │
 │  abi_ui_session_create, abi_ui_node_create,               │
 │  abi_ui_push_event, abi_ui_draw_text, abi_ui_present      │
 ├───────────────────────────────────────────────────────────┤
 │  LAYER 1: C Runtime Engine (~7,000 lines, 12 files)       │
 │  ui_system.c (core session), ui_renderer.c (framebuffer),  │
 │  ui_layout.c (flexbox), ui_host_adapter.c (Win32 GDI),     │
 │  ui_widget.c (immediate-mode widget library)               │
 ├───────────────────────────────────────────────────────────┤
 │  LAYER 0: OS Backend                                      │
 │  Win32 GDI DIB framebuffer, WM_PAINT, BitBlt, stb_truetype │
 │  (GPU backends: Vulkan/D3D12/WebGPU — cataloged, future)  │
 └───────────────────────────────────────────────────────────┘
```

### How a frame is rendered

```
  begin_frame(delta_ms)
    → abi_ui_begin_frame: resets per-frame arena, advances frame counter
  [your draw calls]: abi_ui_draw_rect, abi_ui_draw_text, widget calls
    → Draw commands queued into ring buffer (max 8192 commands)
  end_frame()
    → abi_ui_end_frame: signals frame is ready
  present()
    → abi_ui_present → ui_renderer.c clears framebuffer, replays commands
  host_present()
    → ui_host_adapter.c BitBlt DIB → screen
  host_pump()
    → PeekMessage / DispatchMessage for Win32 input routing
  repeat while !host_should_close(session)
```

**Key capacities** (all power-of-two fixed-size arrays, pre-allocated at session creation):

| Resource | Capacity |
|----------|----------|
| Nodes | 4096 |
| Styles | 8192 |
| State entries | 8192 |
| Draw commands | 8192 |
| Events | 1024 |
| Resources | 2048 |
| Fonts (widget lib) | 8 |
| Sessions | 16 |

**Source files** (in `runtime/native/src/ui/`):

| File | Lines | Role |
|------|-------|------|
| `ui_system.c` | ~2600 | Core session engine: node CRUD, style/state, events, focus, IME, drag-drop, menus, dialogs, clipboard, fonts |
| `ui_system_internal.h` | ~210 | Internal structs (`KainNativeUiNode`, `KainNativeUiSession`) |
| `ui_host_adapter.c` | ~520 | Win32 GDI backend: window create, DIB framebuffer, WM_PAINT BitBlt, message pump, DPI |
| `ui_renderer.c` | ~350 | Software framebuffer: clear, fill rect, rounded rect, glyph text via stb_truetype |
| `ui_layout.c` | ~220 | Flexbox-style layout: direction, padding, spacing, gap |
| `ui_color.c` | ~220 | Color parsing, alpha blending, opacity |
| `ui_runtime.c` | ~1000 | High-level bundle runtime: validation, focus routing, event routing, hot-reload |
| `ui_compiled_bundle.c` | ~610 | JSON bundle deserializer for compiler-compiled trees |
| `ui_hot_reload.c` | ~650 | Shared-memory IPC for live UI reloading |
| `widgets/ui_widget.c` | ~1200 | Immediate-mode widget library: 8 widgets + layout |
| `widgets/ui_widget.h` | ~250 | Widget API header |
| `native_ui_surface.c` | ~280 | KainComponentSurface vtable bridge |

---

## 2. Getting Started

### Minimal Kain App: Window + Frame Loop

Every Kain UI app follows this pattern:

```kain
use std::ui

const WIN_W: Int = 800
const WIN_H: Int = 600

pub fn main() -> Int:
    // 1. Create session + window + attach host
    let session = ui_host_session_create(
        "MyApp", "My Kain App", WIN_W, WIN_H, "winit")

    // 2. Frame loop
    while native_ui_host_should_close(session) == 0:
        // Pump Windows messages (keyboard, mouse, resize)
        let _pump = native_ui_host_pump(session)

        // Begin frame
        let _bf = native_ui_begin_frame(session, 16.0)

        // --- YOUR DRAWING HERE ---

        // End frame + present
        let _fe = native_ui_end_frame(session)
        let _pr = native_ui_present(session)

    // 3. Cleanup
    let _ds = native_ui_session_destroy(session)
    return 0
```

### Drawing a filled rectangle

```kain
// Create a node to represent the rectangle
let node = native_ui_node_create(session, "rect")
native_ui_node_set_rect(session, node, 100.0, 80.0, 200.0, 120.0)

// Set fill color as style (RGBA floats 0.0–1.0)
ui_style_color_rgba(session, node, "fill", 0.13, 0.83, 0.63, 1.0)

// Issue draw command
let _draw = native_ui_draw_rect(session, node, 100.0, 80.0, 200.0, 120.0, "fill")
```

### Drawing text

```kain
// Load a font first
let font_id = native_ui_font_create(session, "default", "Segoe UI", 16.0)

// Create a text node
let tnode = native_ui_node_create(session, "text")
native_ui_node_set_text(session, tnode, "Hello, Kain!")

// Draw it
let _draw = native_ui_draw_text(session, tnode, font_id, 50.0, 100.0,
                                 "Hello, Kain!", "text-primary")
```

### UI shortcut: `ui_host_session_create`

The convenience function handles all 3 init calls in one:

```kain
pub fn ui_host_session_create(
    app_name: String, window_title: String,
    width: Int, height: Int, backend_id: String
) -> Int:
    let session = native_ui_session_create(app_name, width, height)
    let _window = native_ui_window_open(session, window_title, width, height)
    let _host   = native_ui_host_attach(session, backend_id)
    return session
```

---

## 3. The Widget System

Kain's widget library provides **immediate-mode widgets** built on top of the retained-mode ABI. Each widget is a single function call that creates/updates nodes, draws into the framebuffer, handles hover/click/focus state, and returns meaningful data — all in one call.

### Widget lifecycle

```kain
use std::ui
use std::ui::widget

// Create widget context (once, after session creation)
let ctx = widget::create(session)

while running:
    // 1. Begin widget frame (updates mouse state, resets widget counter)
    widget::begin_frame(ctx)

    // 2. Your widgets here — auto-advance layout cursor
    if widget::button(ctx, "Click Me"):
        // handle click
    widget::label(ctx, "Status: OK")
    widget::checkbox(ctx, "Enable", true)

    // 3. End widget frame
    widget::end_frame(ctx)

// Cleanup
widget::destroy(ctx)
```

### All 8 Widgets

#### Button
Clickable with hover/press visual states. Returns `true` on the frame it was clicked (press + release on same widget).

```kain
if widget::button(ctx, "Submit"):
    log("Button clicked!")
```

#### Label
Static text display. Returns the node ID.

```kain
widget::label(ctx, "Hello, World!")
```

#### Checkbox
Togglable square + label. Returns `CheckboxResult` with `.toggled` (`Bool`) and `.value` (`Bool`).

```kain
var music_on = true
let result = widget::checkbox(ctx, "Enable Music", music_on)
if result.toggled:
    music_on = result.value
```

#### Slider
Horizontal track with draggable thumb. Value clamped to `[lo, hi]`. Returns `SliderResult` with `.changed` and `.value`.

```kain
var volume = 0.75
let sr = widget::slider(ctx, volume, 0.0, 1.0)
if sr.changed:
    volume = sr.value
```

#### Textbox
Single-line text input with cursor. Takes current text and max length.

```kain
var text = "edit me"
text = widget::textbox(ctx, text, 64)
```

#### Panel
Titled container with a content area. **Must be paired with `panel_end()`.**

```kain
let _ = widget::panel_begin(ctx, "Settings", 10.0, 50.0, 300.0, 400.0)
    widget::label(ctx, "Inside the panel...")
    widget::button(ctx, "Option A")
widget::panel_end(ctx)
```

#### Progress Bar
Visual progress indicator showing `value / max`.

```kain
let _ = widget::progress(ctx, "Downloading", 45.0, 100.0)
```

#### Window
Draggable, closable floating window container. Returns `Bool` — `false` when the user clicks the close button.

```kain
var win_open = true
win_open = widget::window(ctx, "Stats", 500.0, 300.0, 280.0, 200.0, win_open)
```

### Layout System

The widget library uses an immediate-mode layout system: declare a row or column, then each subsequent widget auto-advances to the next slot.

#### Row layout

```kain
// 3 columns: 100px | 150px | 100px
widget::layout_row(ctx, 3, [100, 150, 100])
widget::button(ctx, "Left")     // column 0
widget::button(ctx, "Center")   // column 1
widget::button(ctx, "Right")    // column 2
```

#### Column layout

```kain
widget::layout_column(ctx, 4, [30, 30, 30, 30])
widget::button(ctx, "Row 1")
widget::button(ctx, "Row 2")
widget::button(ctx, "Row 3")
widget::button(ctx, "Row 4")
```

#### Per-widget sizing

```kain
widget::layout_set_next(ctx, 200, 40)  // override size for next widget only
widget::button(ctx, "Wide Button")
```

### Font Loading (Widget System)

```kain
// Load from explicit path
let arial = widget::load_font(ctx, "C:/Windows/Fonts/arial.ttf", 16.0)

// Load platform default (segoeui.ttf → arial.ttf → tahoma.ttf on Windows)
let default = widget::load_default_font(ctx, 14.0)
```

### Complete Widget Showcase

See `X:/blades/ui_demos/widget_showcase.kn` for a full app using all 8 widgets with panels, layout, fonts, and state management.

---

## 4. The Component System

Kain components bridge the gap between JSX-style declarative UI and the immediate-mode widget system via `std::ui::component`.

### Component bridge overview

The `component.kn` module provides render helpers that translate component concepts into widget library calls:

```
  component render() JSX
         │
         ▼
  std::ui::component.render_button(ctx, label, has_handler, enabled)
         │
         ▼
  std::ui::widget.button(ctx, label)
         │
         ▼
  C abi_ui_widget_button() → framebuffer
```

### Render helpers

```kain
use std::ui::component

// Button — returns true if clicked, signaling the caller to fire its handler
if component::render_button(ctx, "Save", 1, 1):
    fire_save_handler()

// Label — static text
component::render_label(ctx, "Status: Connected")

// Panel begin/end — container with title bar
component::render_panel(ctx, "Log Viewer", 10.0, 50.0, 400.0, 300.0)
component::render_label(ctx, "Line 1: System started")
component::render_label(ctx, "Line 2: All clear")
component::render_panel_end(ctx)

// Checkbox — returns new value (0 or 1)
var checked = 1
checked = component::render_checkbox(ctx, "Enable auto-save", checked)

// Slider — returns (possibly new) Float value
var volume = 0.8
volume = component::render_slider(ctx, volume, 0.0, 1.0)

// Textbox — returns (possibly modified) String
var text = "hello"
text = component::render_textbox(ctx, text, 64)

// Progress bar
component::render_progress(ctx, "Upload", 67.0, 100.0)

// Window — returns new open state (1 = open, 0 = closed)
var win_open = 1
win_open = component::render_window(ctx, "Inspector", 600.0, 100.0,
                                     300.0, 350.0, win_open)
```

### Layout helpers

```kain
component::layout_row(ctx, [100, 150, 100])    // horizontal row
component::layout_column(ctx, [40, 40, 40])    // vertical column
component::layout_set_next(ctx, 200, 50)        // per-widget override
```

### Frame management

```kain
component::begin_frame(ctx)   // updates mouse state, resets widget counter
// ... your widget calls ...
component::end_frame(ctx)     // flushes pending state
```

### Component JSX → Widget mapping

In a Kain `component`, JSX elements map to the widget system:

```kain
component Dashboard:
    state volume: Float = 0.75

    fn render(_self: Self_) -> ComponentOutput:
        render <panel title="Dashboard" x=10 y=10 w=400 h=500>
            <label text="Audio Controls" />
            <slider value={_self.volume} lo=0.0 hi=1.0
                    onChange={(v) => _self.volume = v} />
            <button label="Reset" onClick={() => _self.volume = 0.5} />
        </panel>
```

The component system uses `world` + `surface` wiring to connect to the UI session, and `std::ui::component` render helpers under the hood.

---

## 5. The Font System

Kain's font system is powered by **stb_truetype** (public domain, single-header) and exposed through `std::ui::font`.

### Architecture

```
  Kain code → font::load_ttf() → @extern → C abi_ui_font_load_ttf()
                                              → stbtt_InitFont()
                                              → stores in session resource table (256-entry LRU cache)
```

### Loading fonts

**From file:**
```kain
use std::ui::font

let fid = font::load_from_file(session, "default", "Segoe UI", 16.0,
                                "C:/Windows/Fonts/segoeui.ttf")
```

**From raw TTF data:**
```kain
let ttf_bytes = read_file_bytes("myfont.ttf")
let fid = font::load_ttf(session, "myfont", "Custom Font", 14.0,
                          ttf_bytes, len(ttf_bytes))
```

**Via widget context (convenience):**
```kain
use std::ui::widget
let fid = widget::load_font(ctx, "C:/Windows/Fonts/arial.ttf", 16.0)
let fid2 = widget::load_default_font(ctx, 14.0)  // auto-detects platform font
```

### Getting glyphs

```kain
// Get a rasterized glyph for a Unicode codepoint
let maybe_glyph = font::get_glyph(session, font_id, 0x0041)  // 'A'
match maybe_glyph:
    Some(g):
        // g.bitmap_ptr — raw pointer to alpha mask (width × height bytes)
        // g.width      — bitmap width in pixels
        // g.height     — bitmap height in pixels
        // g.x_offset   — offset from pen origin (typically negative)
        // g.y_offset   — offset from baseline (typically negative)
        // g.advance    — horizontal advance for next glyph
        let w = g.width
        let adv = g.advance
    None:
        // glyph not found
```

### Text measurement

```kain
let w = font::measure_width(session, font_id, "Hello")
let h = font::measure_height(session, font_id, "Hello")
let (w, h) = font::measure(session, font_id, "Hello")  // both at once
```

### Font metrics

```kain
let ascent = font::get_ascent(session, font_id)     // pixels above baseline (> 0)
let descent = font::get_descent(session, font_id)    // pixels below baseline (< 0)
let line_gap = font::get_line_gap(session, font_id)  // recommended line spacing
let (ascent, descent, line_gap) = font::get_vmetrics(session, font_id)
```

### Glyph framebuffer layout

When rendering glyphs manually, position each pixel at:

```
pixel_x = pen_x + glyph.x_offset + col   (0 <= col < glyph.width)
pixel_y = pen_y + glyph.y_offset + row   (0 <= row < glyph.height)
pen_x  += glyph.advance                  // position for next glyph
```

The glyph bitmap is an **alpha mask** (1 byte per pixel):
- `0` = fully transparent
- `255` = fully opaque

Blend over destination: `out = (src_alpha * src_color + (255 - src_alpha) * dst) / 255`

### Font gallery demo

See `X:/blades/ui_demos/font_gallery.kn` — loads 14 Windows fonts, renders each at custom sizes with sample text in panels, with color cycling accents.

### stb_truetype Proof Coverage

The font subsystem has **21 Z3 proof packs** at `extras/_stb-truetype/z3/proofs/` covering:
- Bezier convex hull correctness
- Scale/pixel-height division safety
- Glyph bitmap bounds non-overflow
- hmtx table index bounds
- Scanline AA coverage clamping [0, 255]
- Winding rule accumulation
- Edge clip arithmetic
- Sort comparator total order

---

## 6. The Graphics System

Kain has **two rendering paths**: a software framebuffer (GDI DIB) and a GPU-accelerated path (Vulkan/D3D12/WebGPU). Both are unified under the `KainComponentSurface` vtable. The `std::graphics` module provides the GPU-accelerated rendering primitives.

### Dual-renderer architecture

```
  Kain code
    │
    ├── "winit" backend  ──→ software DIB framebuffer (primary path today)
    │                         abi_ui_draw_rect → ui_renderer.c → BitBlt
    │
    └── "vulkan" backend ──→ GPU-accelerated rendering
                              KainComponentSurface vtable → Vulkan driver
```

### Session management

```kain
use std::graphics

let session = graphics::graphics_session_create("MyApp", 1280, 720)
let backend = graphics::graphics_backend_select(session, "vulkan")

// Query backend status
if graphics::graphics_backend_available("vulkan") == 0:
    log("Vulkan not available — falling back to software")

// Frame loop
while running:
    let _bf = graphics::graphics_begin_frame(session, 16.0)
    // ... draw calls (meshes, pipelines) ...
    let _ef = graphics::graphics_end_frame(session)
    let _pr = graphics::graphics_present(session)

let _ds = graphics::graphics_session_destroy(session)
```

### Shaders (SPIR-V from hex or file)

Shaders are loaded as SPIR-V bytecode. They can come from hex-encoded strings or `.spv` files:

```kain
// From hex (compiler output, inline bytecode)
let vs = graphics::graphics_shader_spirv_from_hex(
    session, "vs_main", "vertex", "main",
    "0x07230203...")   // hex-encoded SPIR-V

// From file (pre-compiled .spv artifact)
let fs = graphics::graphics_shader_spirv_from_file(
    session, "fs_main", "fragment", "main",
    "shaders/lighting.frag.spv")

// Query shader metadata
let stage = graphics::graphics_shader_stage(session, vs)    // "vertex"
let key   = graphics::graphics_shader_key(session, vs)      // "vs_main"
let len   = graphics::graphics_shader_byte_length(session, vs)
```

Shaders can also be stored as **UI resources** via `abi_ui_shader_create`:

```kain
// Store a shader as a UI resource (accessible by any node)
let shader_id = native_ui_shader_create(session, "glow_fs", "fragment", byte_length)
let _bytes = native_ui_resource_set_bytes_hex(session, shader_id, "0x07230203...")
```

### Buffers

```kain
// Create buffer from hex data
let vbuf = graphics::graphics_buffer_create_from_hex(
    session, "vertex", "verts",
    "00000000...",   // hex-encoded bytes
    12)              // element stride

// Query buffer
let len = graphics::graphics_buffer_byte_length(session, vbuf)
let byte = graphics::graphics_buffer_byte_at(session, vbuf, 4)
let kind = graphics::graphics_buffer_kind(session, vbuf)  // "vertex"
```

### Meshes and Pipelines

```kain
let mesh = graphics::graphics_mesh_create(
    session, "cube", vbuf, ibuf, 24, 36)

let pipeline = graphics::graphics_pipeline_create(
    session, "default", vs, fs, "vulkan")

// Issue draw (up to 8192 draw commands per frame)
let _draw = graphics::graphics_draw_mesh(session, pipeline, mesh, 1)

// Inspect draw commands
let cmd_count = graphics::graphics_draw_command_count(session)
let cmd_kind = graphics::graphics_draw_command_kind(session, 0)  // "draw_mesh"
```

### The UI framebuffer

When working with the **software UI renderer** (not GPU), you can access the raw DIB framebuffer:

```kain
let fb_ptr   = ui_fb_ptr(session)       // raw pointer to pixel data
let fb_w     = ui_fb_width(session)
let fb_h     = ui_fb_height(session)
let fb_stride = ui_fb_stride(session)   // bytes per row (typically width * 4)
```

The framebuffer uses **0xAABBGGRR** byte order (BGRA little-endian).

### Color handling

```kain
use std::ui::style

// Packed color constants (0xAARRGGBB)
let bg     = style::COLOR_BG       // 0xFF1A1A24
let accent = style::COLOR_ACCENT   // 0xFF21D4A1
let text   = style::COLOR_TEXT     // 0xFFE8E8F0

// Build custom color
let my_color = style::ui_color_rgba(255, 128, 64, 255)  // orange, fully opaque

// Extract components
let r = style::color_red(my_color)
let g = style::color_green(my_color)

// Linear interpolation between two colors
let mid = style::ui_color_lerp(0xFF0000FF, 0xFF00FF00, 0.5)  // teal

// Convert to float components (for style system)
let (r, g, b, a) = style::ui_color_to_floats(my_color)
```

Default color palette (from `std::ui::style`):

| Constant | Hex | Use |
|----------|-----|-----|
| `COLOR_BG` | `0xFF1A1A24` | Deepest background |
| `COLOR_SURFACE` | `0xFF252540` | Panel surfaces |
| `COLOR_HEADER` | `0xFF1E1E32` | Title bars |
| `COLOR_ACCENT` | `0xFF21D4A1` | Primary accent (teal) |
| `COLOR_ACCENT2` | `0xFF4A90D9` | Secondary accent (blue) |
| `COLOR_ACCENT3` | `0xFFE8914A` | Tertiary accent (orange) |
| `COLOR_ACCENT4` | `0xFFE84A5F` | Destructive (red) |
| `COLOR_TEXT` | `0xFFE8E8F0` | Primary text |
| `COLOR_TEXT_DIM` | `0xFF8888A0` | Muted text |
| `COLOR_BORDER` | `0xFF3A3A5C` | Borders / dividers |
| `COLOR_BUTTON` | `0xFF303050` | Button normal |
| `COLOR_BUTTON_HL` | `0xFF404068` | Button hover |
| `COLOR_BUTTON_PR` | `0xFF505080` | Button pressed |
| `COLOR_INPUT_BG` | `0xFF0A0A14` | Text input background |

---

## 7. The Input System

`std::input` provides a universal input abstraction over keyboard, mouse/pointer, CLI, agent intent, and synthetic sources.

### Source kinds

```kain
use std::input

let src_keyboard = input::input_source_keyboard()    // "human.keyboard"
let src_pointer  = input::input_source_pointer()      // "human.pointer"
let src_cli      = input::input_source_cli()          // "cli.stdin"
let src_ui       = input::input_source_ui_runtime()   // "ui.runtime"
let src_agent    = input::input_source_agent()        // "agent.intent"
let src_synthetic = input::input_source_synthetic()   // "test.synthetic"
```

### Binding actions

```kain
// Bind keyboard keys to named actions
input::input_bind_action(input_session, "human.keyboard",
    "key_down", "Escape", "ui.quit")
input::input_bind_action(input_session, "human.keyboard",
    "key_down", "Space", "game.jump")

// Bind axes (analog input)
input::input_bind_axis(input_session, "human.keyboard",
    "axis", "MouseX", "camera.yaw", 1.0)
```

### Querying input state

```kain
// Per-frame frame begin
input::input_begin_frame(input_session, delta_ms)

// Action queries
if input::input_action_pressed(input_session, "ui.quit"):
    running = false
if input::input_action_down(input_session, "game.jump"):
    player_vy = -10.0

// Axis values
let yaw = input::input_axis_value(input_session, "camera.yaw")

// Text input commits
let commit_count = input::input_text_commit_count(input_session)
var i = 0
while i < commit_count:
    let text = input::input_text_commit(input_session, i)
    append_to_buffer(text)
    i = i + 1
```

### Iterating raw events

```kain
let count = input::input_event_count(input_session)
var i = 0
while i < count:
    let rec = input::input_event_record(input_session, i)
    // rec.source_kind, rec.event_kind, rec.code, rec.action, rec.text
    i = i + 1
```

### UI-specific events (from `std::ui`)

The UI system has its own event ring buffer (1024 events max):

```kain
// In the frame loop, poll UI events
while native_ui_poll_event(session) > 0:
    let kind = native_ui_event_kind(session)
    let target = native_ui_event_target(session)
    let key_code = native_ui_event_key_code(session)
    let text = native_ui_event_text(session)

    if kind == "key_down":
        if cast_to_char(key_code) == 'q':
            running = false
    if kind == "click":
        let x = native_ui_event_x(session)
        let y = native_ui_event_y(session)
        // handle click at (x, y)
```

### Event routing through the node system

```kain
// Hit-test: which node is at (mouse_x, mouse_y)?
let hit_node = native_ui_hit_test(session, mouse_x, mouse_y)

// Push a synthetic event (programmatic input injection)
let _ev = native_ui_push_event(session, "click", hit_node, mouse_x, mouse_y, 0, "")

// Focus management
let _focus = native_ui_focus(session, textbox_node)
let focused = native_ui_focused_node(session)
```

---

## 8. Advanced Demos

The `runtime/native/src/ui/test_ui_v2/` directory contains groundbreaking C demos that push the renderer to its limits. Kain ports exist in `X:/blades/ui_demos/`.

### Cosmic Dashboard (1763 lines C, ~700 KB .exe)

A NASA/JPL mission-control dashboard with:
- **350 parallax particles** with depth and drift
- **Nebula sine-wave gradient** background (deep blues, purples, magentas)
- **6 glass-morphism panels** with colored borders
- **8 fonts** loaded from `C:/Windows/Fonts/`
- **Live waveform display** and rotating particle flux ring
- **Command console**, stellar chart, CPU/MEM gauges
- **Interactive**: click-drag panels, slider for particle speed, keyboard shortcuts (Space=pause, 1-6=toggle, Esc=exit)

### Retro Wave 2084 (1540 lines C, 881 KB .exe)

A synthwave/cyberpunk spectacle with:
- **Scrolling perspective grid** (road-runner style toward viewer)
- **Neon sunset** with horizontal scanlines
- **3-axis wireframe cube** rotating in real time
- **5 transparent glowing panels** (equalizer, wireframe, signal, Matrix rain, clock)
- **3 color schemes**: retrowave, matrix green, ocean blue
- **Multi-pass glow effects**, glitch effect every ~5 seconds
- **Interactive**: slider, button, toggle, textbox, click-drag cassette icons
- **85% pixel animation rate** (Oracle-verified)

### 3D UI Sandbox (1330 lines C, 881 KB .exe)

- **3D rotation matrices** with perspective/orthographic projection
- **Painter's algorithm** depth sorting
- **Isometric grid floor**, Z-depth floating panels
- **Particle fountain** with gravity
- **120-star parallax starfield**
- **Exploding cube animation**
- **DIB re-creation on window resize**

### Kain Ports (in `X:/blades/ui_demos/`)

| Demo | File | What It Shows |
|------|------|---------------|
| **Widget Showcase** | `widget_showcase.kn` | All 8 widgets, layout, fonts, state management |
| **Retro Wave Lite** | `retrowave_lite.kn` | Framebuffer pixel art, stars, grid, equalizer, slider |
| **Font Gallery** | `font_gallery.kn` | 14 fonts loaded, sample text rendering, color cycling |

### Porting C demos to Kain

The key difference when porting:
1. **Replace `#include` headers** with `use std::ui` and `use std::ui::widget`
2. **Replace `KainWin32UiHost*` direct framebuffer access** with `ui_fb_ptr(session)` and related helpers
3. **Replace C widget calls** with Kain `widget::button(ctx, ...)` etc.
4. **Replace Win32 message pump** with `native_ui_host_pump(session)`
5. **Replace manual `CreateWindowEx`** with `ui_host_session_create()` convenience

---

## 9. Building & Running

### The Makefile system (C layer)

The C runtime is built from `runtime/native/src/ui/Makefile`:

```bash
make              # static lib (libkain_ui.a) + tests + demos
make -j8          # parallel build
make static       # build libkain_ui.a only (599 KB, 12 source files)
make demos        # build test_ui_v2/ executables
make run_cosmic   # build + run cosmic dashboard
make run_retro    # build + run retro wave
make run_3d       # build + run 3D sandbox
make clean
```

**Architecture**: Incremental compilation with auto-generated `.d` dependency files. Each `build.bat` in test directories works for standalone MSVC builds.

### Linking against the runtime

Kain applications link against the native runtime via the Bazel build system:

```
bazel build //:kain    # builds kain.exe + runtime
kain build my_app.kn --target llvm
```

The `kain build` command handles linking against `kain_runtime.lib` automatically.

### Kain application build flow

```bash
# Typecheck
kain check my_ui_app.kn

# Build to native executable
kain build my_ui_app.kn --target llvm

# Run directly
kain run my_ui_app.kn --target llvm
```

### Running the Kain demos

```bash
kain run blades/ui_demos/widget_showcase.kn --target llvm
kain run blades/ui_demos/retrowave_lite.kn --target llvm
kain run blades/ui_demos/font_gallery.kn --target llvm
```

---

## 10. Architecture Deep Dive

### Retained-mode vs Immediate-mode

The Kain UI system bridges **both** paradigms:

| Layer | Paradigm | How It Works |
|-------|----------|-------------|
| **Core engine** (`ui_system.c`) | **Retained-mode** | Nodes persist in a tree across frames; styles, state, flags are accumulated. `begin_frame` resets per-frame arena; `end_frame` signals completion. Draw commands are queued into a ring buffer. |
| **Widget library** (`ui_widget.c`) | **Immediate-mode** | Each widget call creates/updates nodes, draws, and tracks interaction state in a single function. No persistent widget objects — the code IS the UI. |
| **Component system** (`component.kn`) | **Bridge** | Declarative JSX components reconcile against the retained-mode tree via stable keys (`ui_reconcile_node`), then render through the widget library's immediate-mode calls. |
| **Hot-reload** (`ui_hot_reload.c`) | **Live** | Shared-memory IPC channel; `abi_ui_hot_reload_begin` / `abi_ui_hot_reload_commit` transfer state across hot-reloads with file signature watching. |

### How the widget library bridges both paradigms

The widget library's immediate-mode API internally calls retained-mode ABI functions:

```
  widget::button(ctx, "Click")
    → ui_widget.c:
       1. Generate stable key: "wbtn_N" (N = widget counter)
       2. abi_ui_node_find_by_stable_key(session, "wbtn_N") — find or create
       3. abi_ui_node_set_rect(session, node, ...) — update position from layout cursor
       4. Check mouse position vs node rect for hover/press detection
       5. Draw into framebuffer: fill rect, border, text glyphs
       6. Advance layout cursor (ctx->layout_x += width + spacing)
       7. Return 1 if clicked (press + release on same node)
```

This gives you the developer ergonomics of immediate-mode (no callbacks, no widget objects, code IS layout) while keeping the engine benefits of retained-mode (stable node identity across frames, efficient diffing, hot-reload compatibility).

### DPI scaling end-to-end

```
  SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)
    │ (loaded defensively via GetProcAddress)
    ▼
  GetDeviceCaps(dc, LOGPIXELSX) / 96.0f
    → host->dpi_scale (e.g. 2.0 on 4K @ 200%)
    │
    ▼
  session->dpi_scale = host->dpi_scale
    │
    ▼
  ui_render_node():
    render_x = node->x * session->dpi_scale
    render_y = node->y * session->dpi_scale
    render_w = node->width  * session->dpi_scale
    render_h = node->height * session->dpi_scale
    │
    ▼
  Widget context also tracks:
    ctx->dpi_scale
```

**Result**: All coordinates in Kain code are in **logical pixels**. The engine multiplies by `dpi_scale` automatically. On a 4K display at 200%, a 100×30 button renders at 200×60 physical pixels.

### WM_PAINT, WM_SIZE, WM_DPICHANGED flow

```
  WM_SIZE:
    1. SelectObject(old_dib)  — detach old DIB
    2. DeleteObject(old_hbitmap)
    3. CreateDIBSection(new_width, new_height) → new framebuffer
    4. memset(new_fb, 0, new_width * new_height * 4)
    5. Update host->width, host->height, host->framebuffer

  WM_PAINT:
    1. BeginPaint → get HDC
    2. BitBlt(host->framebuffer DIB → window DC)
    3. EndPaint → validate window

  WM_DPICHANGED:
    1. Get suggested rect from lParam
    2. SetWindowPos(suggested_rect) → auto-triggers WM_SIZE
    3. WM_SIZE handler creates new DIB at new resolution
```

### The full frame loop lifecycle

```
  1. host_pump()                 → PeekMessage + DispatchMessage (fills input events)
  2. begin_frame(delta_ms)       → reset per-frame arena, advance frame counter
  3. [user draw calls]           → queue draw commands, create/update nodes
  4. end_frame()                 → signal frame complete
  5. present()                   → ui_renderer.c clears framebuffer, replays draw commands
  6. host_present()              → BitBlt DIB → screen (if attached; auto-called by present)
  7. host_should_close() check   → poll WM_CLOSE flag
  8. Sleep(~16ms) or vsync       → target 60 FPS
  9. goto 1
```

### Node tree and stable keys

Nodes form a persistent tree across frames. Stable keys enable **reconciliation** — finding existing nodes or creating new ones:

```kain
// Find existing node or create one with this stable key
let node = ui_reconcile_node(session, parent_id, "rect", "my_button", x, y, w, h)

// This expands to:
let existing = native_ui_node_find_by_stable_key(session, "my_button")
if existing > 0:
    // Update position/size of existing node
    native_ui_node_set_parent(session, existing, parent_id)
    native_ui_node_set_rect(session, existing, x, y, w, h)
    return existing
// Create new
let node = native_ui_node_create(session, "rect")
native_ui_node_set_stable_key(session, node, "my_button")
native_ui_node_set_parent(session, node, parent_id)
native_ui_node_set_rect(session, node, x, y, w, h)
return node
```

### State and style system

Each node has key-value stores for styles and state:

```kain
// Styles (visual properties, designed to be inherited)
native_ui_node_set_style_i64(session, node, "font-size", 16)
native_ui_node_set_style_f64(session, node, "opacity", 0.85)
native_ui_node_set_style_string(session, node, "font-family", "Segoe UI")

// State (interaction / application data)
native_ui_node_set_state_i64(session, node, "counter", 42)
native_ui_node_set_state_f64(session, node, "scroll_offset", 120.5)
native_ui_node_set_state_string(session, node, "user.name", "Alice")
```

Helper wrappers in `std::ui` make this ergonomic:

```kain
ui_state_set_bool(session, node, "visible", 1)
ui_state_set_f64(session, node, "volume", 0.75)
ui_state_counter(session, node, "clicks", 1)    // increment atomically
ui_state_toggle(session, node, "expanded")       // flip boolean

// Style color
ui_style_color_rgba(session, node, "fill", 0.2, 0.8, 0.4, 1.0)
ui_style_padding(session, node, "container", 8.0, 4.0, 8.0, 4.0)
```

### Known limitations

1. **Single-threaded** — All rendering and event processing is single-threaded
2. **Win32-only host** — The "winit" backend is Windows GDI only. Linux/macOS host adapters are future work
3. **GPU backends cataloged** — Vulkan, D3D12, and WebGPU backends exist architecturally (see Section 11). The `KainComponentSurface` vtable, Vulkan ABI loader, and graphics bundle system are built. The software DIB framebuffer is the primary rendering path today.
4. **No ClearType** — stb_truetype basic hinting is used; no sub-pixel rendering
5. **No theming engine** — Widget colors and sizes are hardcoded; custom theming requires direct style manipulation
6. **Fixed-size arrays** — All resource pools are fixed at compile time (4096 nodes, 8192 styles, etc.); no dynamic growth

---

## 11. GPU Rendering & Vulkan Integration

Kain's UI system has a **dual-renderer architecture**: the software DIB framebuffer is the primary path, but the system is fully architected for GPU-accelerated rendering through a pluggable vtable system.

### The KainComponentSurface vtable

The `KainComponentSurface` struct (`component_surface.h`) is the **ABI contract** between the Kain compiler and any rendering backend. It's a trait-style vtable with 19 method slots:

```c
typedef struct KainComponentSurface {
    // Session lifecycle
    int64_t (*session_create)(const char* name, int64_t width, int64_t height);
    void    (*session_destroy)(int64_t session_id);

    // Element tree (abstract — "kind" is surface-interpreted)
    int64_t (*element_begin)(int64_t session_id, int64_t parent_id,
                             const char* kind, const char* stable_key);
    void    (*element_end)(int64_t session_id, int64_t element_id);
    void    (*element_set_text)(int64_t session_id, int64_t element_id,
                                const char* text);

    // Style/attribute setters (i64, f64, string)
    void    (*element_set_attr_i64)(...);
    void    (*element_set_attr_f64)(...);
    void    (*element_set_attr_string)(...);

    // State persistence
    int64_t (*state_get_i64)(...);
    void    (*state_set_i64)(...);

    // Frame lifecycle
    void    (*begin_frame)(...);
    void    (*end_frame)(...);
    void    (*present)(...);

    // Events & window
    int64_t (*poll_event)(...);
    int64_t (*should_close)(...);
    int64_t (*window_open)(...);
    int64_t (*host_pump)(...);

    // Platform handle (HWND, Display*, CAMetalLayer*)
    void    (*session_attach_platform)(...);

    // GPU extension (slot 18)
    const KainGpuSurfaceExtension* (*get_gpu_extension)(int64_t session_id);
} KainComponentSurface;
```

**Key insight**: The compiler emits calls through this vtable; the backend implements them. Neither side knows the other's internals. A "native_ui" surface wraps `ui_system.h` for software rendering; a "vulkan" surface wraps the Vulkan driver. The compiler codegen resolves the surface once at frame-loop init, then calls through the vtable every frame.

### Backend registration

Backends register themselves at startup:

```c
// In vulkan_surface_shim.c (or any backend):
kain_component_surface_register("vulkan", &vulkan_surface);

// In ui_host_adapter.c (host attach path):
kain_component_surface_register("native_ui", &native_ui_surface);
kain_component_surface_register("winit", &native_ui_surface);  // alias

// Resolution at runtime:
const KainComponentSurface* surface = kain_component_surface_resolve("vulkan");
if (surface != NULL) {
    int64_t gpu_session = surface->session_create(app_name, width, height);
    // ... use vtable for all rendering
}
```

### How `host_attach` resolves GPU backends

When you call `abi_ui_host_attach(session, "vulkan")`, the host adapter:

```c
// ui_host_adapter.c — abi_ui_host_attach()
if (strcmp(backend_id, "vulkan") == 0) {
    const KainComponentSurface* surface =
        kain_component_surface_resolve("vulkan");
    if (surface == NULL) return ABI_UI_INVALID_ARGUMENT;

    int64_t vulkan_session = surface->session_create(
        session->window_title, session->width, session->height);

    session->host_backend = "vulkan";
    session->component_surface = surface;
    session->component_session_id = vulkan_session;
    session->host_attached = 1;
    return ABI_UI_OK;
}
```

Supported backend IDs: `"winit"`, `"vulkan"`, `"d3d12"`, `"webgpu"`.

### The RENDERER_BACKEND environment variable

The `RENDERER_BACKEND` env var controls which backend is used at runtime:

```bash
# Software rendering (default)
set RENDERER_BACKEND=winit
kain run my_app.kn --target llvm

# GPU rendering (when available)
set RENDERER_BACKEND=vulkan
kain run my_app.kn --target llvm
```

The `renderer_backend.h` header defines the catalog:

```c
typedef enum {
    KAIN_RENDERER_BACKEND_UNKNOWN = 0,
    KAIN_RENDERER_BACKEND_VULKAN,
    KAIN_RENDERER_BACKEND_D3D12,
    KAIN_RENDERER_BACKEND_WEBGPU,
} KainRendererBackendKind;

typedef struct {
    KainRendererBackendKind kind;
    const char* id;           // "vulkan", "d3d12", "webgpu"
    const char* display_name; // "Vulkan 1.3"
    const char* runtime_name; // "libkain-vulkan-abi"
    const char* service_key;  // "kain.vulkan.abi"
    const char* summary;      // human-readable description
    int available;            // 1 if detected at startup
} KainRendererBackendDescriptor;
```

### The GPU Surface Extension

GPU backends expose a `KainGpuSurfaceExtension` via slot 18 of the vtable:

```c
typedef struct KainGpuSurfaceExtension {
    // Load a fragment shader from hex-encoded SPIR-V.
    // Creates render pass, descriptor set layout, pipeline layout,
    // graphics pipeline (with embedded fullscreen-triangle VS),
    // descriptor pool, uniform buffers, and descriptor writes.
    int64_t (*load_shader)(int64_t session_id, const char* spirv_hex);

    // Update uniform buffer bindings before each frame:
    //   binding 0 = time (Float, 4 bytes)
    //   binding 1 = resolution (Vec2, 8 bytes)
    //   binding 2 = mouse (Vec2, 8 bytes)
    int64_t (*set_uniform)(int64_t session_id, uint32_t binding,
                            const void* data, uint64_t size);
} KainGpuSurfaceExtension;
```

Software backends (GDI) return `NULL` for `get_gpu_extension()`. GPU backends (Vulkan, D3D12, WebGPU) return a fully populated extension.

### Shaders as UI Resources

The UI system stores shaders as **resources** alongside fonts and textures. Each resource has a `resource_type` field that can be `"shader"`, `"font"`, `"texture"`, or `"canvas"`:

```kain
// Create a shader resource in the UI session
let shader_id = native_ui_shader_create(session, "bloom_fs", "fragment", byte_len)

// Upload SPIR-V bytecode as hex
native_ui_resource_set_bytes_hex(session, shader_id, "0x07230203...")

// Query shader metadata
let rtype = native_ui_resource_type(session, shader_id)     // "shader"
let rkey  = native_ui_resource_key(session, shader_id)       // "bloom_fs"
let rlen  = native_ui_resource_byte_length(session, shader_id)
```

### The Vulkan ABI Loader (`extras/vulkan-abi/`)

The Vulkan ABI loader is a **separately-linked shared library** (`libkain-vulkan-abi.dll` on Windows, `.so` on Linux) that owns ALL actual Vulkan driver calls. It uses dynamic loading so that no Vulkan SDK is needed at compile time.

**Architecture**:

```
vulkan_surface_shim.c (runtime contract)
    │ dlopen("libkain-vulkan-abi.dll")
    │ dlsym("kain_vulkan_abi_get_vtable")
    ▼
vulkan_abi.c (~2,050 lines — this library)
    │ dlopen("vulkan-1.dll" / "libvulkan.so.1" / "libMoltenVK.dylib")
    │ 43 PFNs resolved via vkGetInstanceProcAddr
    │ KainComponentSurface vtable filled with real Vulkan calls
    │ All 19 vtable slots implemented
    ▼
Vulkan driver (vendor ICD)
```

**Critical design rules**:
- **NEVER** includes `<vulkan/vulkan.h>` — all Vulkan types are `uintptr_t`
- **NEVER** links the Vulkan SDK at compile time — purely runtime `dlopen`
- All `Vk*CreateInfo` structs are built with hardcoded `sType` values
- PFN resolution is split: instance-level after `vkCreateInstance`, device-level after `vkCreateDevice`

**43 PFNs resolved**, covering: instance/device creation, WSI surfaces (Win32/X11/Wayland/MoltenVK), swapchain lifecycle, command buffers/pools, semaphores, fences, image views, queue submission + present.

**Supported platforms**:

| Platform | Surface Extension | Native Handle |
|----------|-------------------|---------------|
| Windows | `VK_KHR_win32_surface` | `HINSTANCE` + `HWND` |
| Linux (X11) | `VK_KHR_xlib_surface` | `Display*` + `Window` |
| Linux (Wayland) | `VK_KHR_wayland_surface` | `wl_display*` + `wl_surface*` |
| macOS | `VK_MVK_macos_surface` | `CAMetalLayer*` (via MoltenVK) |

### The Graphics Bundle System

The `graphics_bundle.h` header defines a complete GPU pipeline descriptor system. A **graphics bundle** is a JSON sidecar that describes an entire rendering/compute pipeline:

```
  .realtime_app.json sidecar
       │
       ▼
  KainRuntimeGraphicsBundle
       │
       ├── Material Plans (shader refs, resource bindings)
       ├── Compute Plans (dispatch/workgroup sizes, tensor/neural bindings)
       ├── Render Graph Contract (passes, attachments, dependencies)
       ├── Residency Contract (GPU-only, CPU-to-GPU, readback, transient pool)
       └── Compute Schedule (steps, barriers, async queues)
```

**Key structures**:

| Structure | Purpose | Max Counts |
|-----------|---------|------------|
| `KainRuntimeGraphicsMaterialPlan` | Shader + resource binding per material | 8 bindings |
| `KainRuntimeGraphicsComputePlan` | Compute shader dispatch, workgroup sizes, tensor/neural bindings | 8 bindings |
| `KainRuntimeGraphicsRenderGraphContract` | Render passes, color/depth/storage attachments, dependencies | 8 passes, 12 attachments, 12 deps |
| `KainRuntimeGraphicsResidencyContract` | Memory residency: GPU-only, CPU-to-GPU, readback, transient pool | 16 resources |
| `KainRuntimeGraphicsComputeSchedule` | Compute steps with barriers, async queues | 8 steps, 12 barriers |

**GPU runtime library** (`kain_gpu_runtime.dll`):

```c
// Dynamic library entry points
typedef void* (*KainGpuRuntimeCreateFn)(const void* config);
typedef int (*KainGpuRuntimeDispatchFn)(void* handle,
    const KainGpuRuntimeDispatchRequest* request,
    KainGpuRuntimeDispatchResult* result);
typedef void (*KainGpuRuntimeDestroyFn)(void* handle);

// Dispatch request includes shader bundle path, residency path,
// compute key, dispatch size, and barrier metadata JSON.
typedef struct {
    const char* shader_bundle_path;
    const char* compute_residency_path;
    const char* compute_key;
    unsigned int dispatch_size[3];
    const char* barrier_json;   // NULL = full pipeline drain fallback
} KainGpuRuntimeDispatchRequest;
```

### Platform surface handle

The `KainPlatformSurfaceHandle` bridges platform-native window handles across OSes:

```c
typedef struct KainPlatformSurfaceHandle {
#ifdef _WIN32
    void* hinstance;  // HINSTANCE
    void* hwnd;       // HWND
#elif defined(__linux__) && defined(VK_USE_PLATFORM_WAYLAND_KHR)
    void* wl_display; // struct wl_display*
    void* wl_surface; // struct wl_surface*
#elif defined(__linux__)
    void* x11_display;  // Display*
    uintptr_t x11_window; // Window
#elif defined(__APPLE__)
    void* metal_layer; // CAMetalLayer*
#endif
} KainPlatformSurfaceHandle;
```

### GPU resource linking in Kain

The `std::ui` module includes helpers for linking GPU resources to UI nodes:

```kain
use std::ui

// Link a GPU shared buffer to a node (for compute → render handoff)
let result = ui_state_shared_buffer_resource(
    session, node_id, gpu_buffer_view, resource_id)

// Link a GPU shared image to a node (for texture display)
let result = ui_state_shared_image_resource(
    session, node_id, gpu_image_view, resource_id)
```

### Current state of GPU backends

| Component | Status |
|-----------|--------|
| `KainComponentSurface` vtable | **Live** — used by software renderer today |
| Vulkan ABI loader (`vulkan_abi.c`) | **Built** — ~2,050 lines, 43 PFNs, all 19 vtable slots, dlopen-based |
| D3D12 backend | **Cataloged** — `KAIN_RENDERER_BACKEND_D3D12` enum exists, no implementation yet |
| WebGPU backend | **Cataloged** — `KAIN_RENDERER_BACKEND_WEBGPU` enum exists, no implementation yet |
| `KainGpuSurfaceExtension` | **Defined** — `load_shader` + `set_uniform` slots are implemented by Vulkan backend |
| Graphics bundle system | **Built** — JSON loader, render graph, residency, compute schedule validation |
| GPU runtime library | **Cataloged** — `kain_gpu_runtime.dll` entry points defined |
| `std::graphics` module | **Live** — full Kain bindings for buffers, shaders, meshes, pipelines |
| Software DIB renderer | **Primary path** — fully functional, Oracle-verified |

### Choosing your rendering path

```kain
// Software path (default, always works):
let session = ui_host_session_create("App", "My App", 800, 600, "winit")

// GPU path (requires Vulkan driver + Vulkan ABI loader built):
let session = ui_host_session_create("App", "My App", 800, 600, "vulkan")

// Query what's available:
if graphics::graphics_backend_available("vulkan") != 0:
    log("Vulkan ready")
else:
    log("Falling back to software")

// Check active backend:
let active = graphics::graphics_active_backend(session)  // "vulkan" or empty
let status = graphics::graphics_backend_status("vulkan")  // human-readable status
```

## Quick Reference

### Common imports

```kain
use std::ui                // Core UI ABI (session, nodes, events, rendering)
use std::ui::widget        // Widget library (button, slider, panel, etc.)
use std::ui::style         // Color constants, sizes, helpers
use std::ui::font          // Font loading, glyph access, text measurement
use std::ui::component     // Component bridge (render helpers)
use std::input             // Input system (keyboard, mouse, bindings)
use std::graphics          // GPU graphics (buffers, shaders, pipelines)
```

### The canonical frame loop

```kain
use std::ui
use std::ui::widget

pub fn main() -> Int:
    let session = ui_host_session_create("App", "Kain App", 800, 600, "winit")
    let ctx = widget::create(session)
    widget::load_default_font(ctx, 14.0)

    while native_ui_host_should_close(session) == 0:
        let _pump = native_ui_host_pump(session)
        let _bf = native_ui_begin_frame(session, 16.0)
        widget::begin_frame(ctx)

        // ── YOUR UI HERE ──
        if widget::button(ctx, "Hello"):
            widget::label(ctx, "World!")

        widget::end_frame(ctx)
        let _fe = native_ui_end_frame(session)
        let _pr = native_ui_present(session)

    widget::destroy(ctx)
    let _ds = native_ui_session_destroy(session)
    return 0
```

### Key constants (from `std::ui::style`)

| Constant | Value |
|----------|-------|
| `BUTTON_WIDTH` | 100 |
| `BUTTON_HEIGHT` | 30 |
| `CHECKBOX_SIZE` | 18 |
| `SLIDER_WIDTH` | 200 |
| `SLIDER_HEIGHT` | 20 |
| `TEXTBOX_WIDTH` | 160 |
| `TEXTBOX_HEIGHT` | 26 |
| `LABEL_HEIGHT` | 20 |
| `PROGRESS_WIDTH` | 150 |
| `PROGRESS_HEIGHT` | 18 |
| `PADDING` | 8 |
| `SPACING` | 4 |

---

## Further Reading

- **C Runtime README**: `X:/runtime/native/src/ui/README.md` — full engine architecture
- **Public ABI header**: `X:/runtime/native/include/ui_system.h` — all 90+ C API functions
- **Font ABI header**: `X:/runtime/native/include/ui_font.h` — glyph and font API
- **Widget C API**: `X:/runtime/native/src/ui/widgets/ui_widget.h` — widget library header
- **Kain stdlib source**: `X:/stdlib/ui.kn` (1677 lines), `X:/stdlib/ui/widget.kn`, `X:/stdlib/ui/font.kn`, `X:/stdlib/ui/style.kn`, `X:/stdlib/ui/component.kn`
- **Reference C demos**: `X:/runtime/native/src/ui/test_ui_v2/` (cosmic_dashboard.c, retrowave.c, ui3d_sandbox.c)
- **Kain UI demos**: `X:/blades/ui_demos/` (widget_showcase.kn, retrowave_lite.kn, font_gallery.kn)
- **Kain by Example** (all language features): `X:/docs/KAIN_BY_EXAMPLE.md`
- **Graphics system**: `X:/stdlib/graphics.kn`, `X:/stdlib/graphics/shared.kn`
- **Input system**: `X:/stdlib/input.kn`

### GPU / Vulkan references
- **Component surface vtable**: `X:/runtime/native/include/component_surface.h` — 19-slot rendering backend trait
- **GPU surface extension**: `X:/runtime/native/include/gpu_surface_extension.h` — `load_shader` + `set_uniform` extension
- **Renderer backend catalog**: `X:/runtime/native/include/renderer_backend.h` — Vulkan/D3D12/WebGPU enum
- **Graphics system ABI**: `X:/runtime/native/include/graphics_system.h` — buffers, shaders, meshes, pipelines API
- **Graphics bundle system**: `X:/runtime/native/include/graphics_bundle.h` — render graph, residency, compute schedule contracts
- **Vulkan ABI loader**: `X:/runtime/native/extras/vulkan-abi/` — ~2,050 lines, 43 PFNs, 19 vtable slots, dlopen-based
- **Research docs**: `X:/research/ui/std_ui_expansion.md`, `X:/research/ui/implementation_tasks.md`
