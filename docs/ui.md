# Kain UI -- Complete Guide

**The single source of truth for writing UI in Kain.** From C rendering substrate to high-level Kain components, this document covers every layer of the Kain UI stack.

**Last updated:** 2026-06-26
**Covers:** KUIF Phases 1-4 (C substrate extraction, compiler pipeline, Kain widget library)

---

## 1. Architecture Overview

Kain's UI system is a **4-layer stack** built on a clean C11 rendering substrate wrapped by Kain `component` definitions with compiler-owned semantic state.

```
 ┌───────────────────────────────────────────────────────────┐
 │  LAYER 3: Kain Components & Widgets                       │
 │  component Button ... <box><text>...</text></box>        │
 │  HStack / VStack / ZStack / Grid layout system           │
 │  std::ui, std::ui::theme, std::ui::core                  │
 ├───────────────────────────────────────────────────────────┤
 │  LAYER 2: Component Surface Vtable (compiler-emitted)     │
 │  KainComponentSurface — 24-slot ABI contract              │
 │  JSX → vtable call lowering (element_begin, set_attr...) │
 │  State persistence: i64 / f64 / String                    │
 │  Callback binding: element_set_callback (slot 23)         │
 ├───────────────────────────────────────────────────────────┤
 │  LAYER 1: C Rendering Substrate (~1,500 loc, 7 files)     │
 │  kain_geometry, kain_render_software (16 primitives),    │
 │  kain_compositor, kain_input, kain_font,                 │
 │  kain_surface, kain_host (vtable)                         │
 ├───────────────────────────────────────────────────────────┤
 │  LAYER 0: OS Backend                                      │
 │  Win32 GDI DIB framebuffer, WM_PAINT, BitBlt, stb_truetype │
 │  (GPU backends: Vulkan/D3D12/WebGPU — built, future path) │
 └───────────────────────────────────────────────────────────┘
```

### Design Philosophy

Kain owns widgets, layout, state, reactivity, animation, and theming. C provides **only** draw primitives, damage tracking, input events, font rasterization, and a platform host interface.

The `KainComponentSurface` vtable is the ABI contract between the Kain compiler and any rendering backend. The compiler emits calls through this vtable; the backend implements them. Neither side knows the other's internals.

---

## 2. The C Substrate (Layers 0-1)

### 2.1 Layer Map

The C substrate lives in `runtime/native/src/ui/kain/` with twin headers in `runtime/native/include/`:

| File | Lines | Role |
|------|-------|------|
| `kain_geometry.h` | ~200 | Primitive types: `kainRect`, `kainPoint`, `kainSize`, `kainColor`, `kainMatrix` + 23 pure-math helpers (intersect, union, contains, transform, lerp, blend) |
| `kain_render_software.h` | ~85 | 16 draw primitive signatures + clip/transform stack |
| `kain_render_software.c` | ~500 | 16 primitives extracted from `ui_renderer.c` — no tree-walking, no widgets |
| `kain_compositor.h` | ~50 | Damage region tracker: per-frame dirty rect accumulator (max 64 rects) |
| `kain_compositor.c` | ~150 | Union-rect computation, frame-bounded lifecycle |
| `kain_input.h` | ~100 | 11 event kind enum, `KainInputEvent` struct, typed pipeline over `ui_system` event queue |
| `kain_input.c` | ~120 | Thin wrapper over `abi_ui_push_event`/`abi_ui_poll_event`, hit-test delegation |
| `kain_font.h` | ~55 | Font load (bytes, path, platform default), glyph access, text measurement |
| `kain_font.c` | ~200 | Font path search extracted from `ui_widget.c`; wraps `abi_ui_font_load_ttf` |
| `kain_surface.h` | ~70 | GPU surface abstraction (software/vulkan/d3d12/webgpu), pixel access, resize |
| `kain_host.h` | ~100 | Platform-agnostic host vtable: window lifecycle, framebuffer access, message pump, clipboard, cursor, GPU surface extension |
| `kain_host_win32.c` | ~600 | Win32 GDI backend: DIB framebuffer, window class, message pump, BitBlt present |

### 2.2 Geometry Types (`kain_geometry.h`)

All coordinates are `float` for GPU compatibility. Colors are `float [0..1]` RGBA.

```c
typedef struct kainRect  { float x, y, w, h; } kainRect;
typedef struct kainPoint { float x, y; }        kainPoint;
typedef struct kainSize  { float w, h; }        kainSize;
typedef struct kainColor { float r, g, b, a; }  kainColor;

// 2D affine matrix, row-major: [a b tx; c d ty; 0 0 1]
typedef struct kainMatrix { float m[6]; } kainMatrix;
```

**Operations:** `kain_rect_make`, `kain_rect_contains`, `kain_rect_overlaps`, `kain_rect_intersect`, `kain_rect_union`, `kain_point_make/add/sub`, `kain_size_make`, `kain_color_rgba`, `kain_color_from_u32`/`kain_color_to_u32`, `kain_color_lerp`, `kain_color_clamp`, `kain_matrix_identity/translate/scale/rotate/mul/transform_point`.

### 2.3 Software Renderer (`kain_render_software.h`)

16 backend-agnostic draw primitives. No tree-walking, no widgets, no layout.

```c
// Lifecycle
KainSoftwareRenderer* kain_renderer_create(int fb_width, int fb_height, uint32_t* framebuffer);
void kain_renderer_destroy(KainSoftwareRenderer* r);
void kain_renderer_set_framebuffer(KainSoftwareRenderer* r, uint32_t* fb, int w, int h);
void kain_renderer_set_font_session(KainSoftwareRenderer* r, int64_t session_id);

// Frame lifecycle
void kain_renderer_clear(KainSoftwareRenderer* r, kainColor color);
void kain_renderer_submit(KainSoftwareRenderer* r);
void kain_renderer_present(KainSoftwareRenderer* r);

// Draw primitives (16)
void kain_render_fill_rect(KainSoftwareRenderer* r, kainRect rect, kainColor color);
void kain_render_fill_rounded_rect(KainSoftwareRenderer* r, kainRect rect, float radius, kainColor color);
void kain_render_stroke_rect(KainSoftwareRenderer* r, kainRect rect, float thickness, kainColor color);
void kain_render_fill_circle(KainSoftwareRenderer* r, kainPoint center, float radius, kainColor color);
void kain_render_stroke_circle(KainSoftwareRenderer* r, kainPoint center, float radius, float thickness, kainColor color);
void kain_render_blit(KainSoftwareRenderer* r, kainRect src, kainRect dst, int64_t texture_id);
void kain_render_text(KainSoftwareRenderer* r, kainPoint pos, const char* text, int64_t font_id, float size, kainColor color);
void kain_render_gradient_rect(KainSoftwareRenderer* r, kainRect rect, const kainColor* colors, const float* stops, int count);
void kain_render_blur(KainSoftwareRenderer* r, kainRect rect, float radius);
void kain_render_push_clip(KainSoftwareRenderer* r, kainRect rect);
void kain_render_pop_clip(KainSoftwareRenderer* r);
void kain_render_push_transform(KainSoftwareRenderer* r, kainMatrix matrix);
void kain_render_pop_transform(KainSoftwareRenderer* r);
```

### 2.4 Compositor (`kain_compositor.h`)

Tracks dirty (damaged) rectangles so the renderer can skip redrawing unchanged regions.

```c
KainCompositor* kain_compositor_create(int fb_width, int fb_height);
void            kain_compositor_destroy(KainCompositor* c);
void            kain_compositor_begin_frame(KainCompositor* c);
void            kain_compositor_end_frame(KainCompositor* c);
void            kain_compositor_damage_rect(KainCompositor* c, float x, float y, float w, float h);
void            kain_compositor_damage_node(KainCompositor* c, int64_t node_id);
kainRect        kain_compositor_damaged_region(KainCompositor* c);
bool            kain_compositor_has_damage(KainCompositor* c);
void            kain_compositor_clear_damage(KainCompositor* c);
```

### 2.5 Input Pipeline (`kain_input.h`)

Typed wrapper over the existing `abi_ui_push_event`/`abi_ui_poll_event` event queue.

| Enum | Value | Event |
|------|-------|-------|
| `KAIN_INPUT_KEY_DOWN` | 1 | Keyboard key pressed |
| `KAIN_INPUT_KEY_UP` | 2 | Keyboard key released |
| `KAIN_INPUT_TEXT` | 3 | Unicode text input |
| `KAIN_INPUT_POINTER_DOWN` | 4 | Mouse button down |
| `KAIN_INPUT_POINTER_UP` | 5 | Mouse button up |
| `KAIN_INPUT_POINTER_MOVE` | 6 | Mouse moved |
| `KAIN_INPUT_POINTER_WHEEL` | 7 | Mouse wheel |
| `KAIN_INPUT_FOCUS_IN` | 8 | Element gained focus |
| `KAIN_INPUT_FOCUS_OUT` | 9 | Element lost focus |
| `KAIN_INPUT_DRAG` | 10 | Drag operation |
| `KAIN_INPUT_DROP` | 11 | Drop operation |

**Hit testing:** `kain_input_hit_test(pipeline, x, y)` returns the node_id at point (x,y) or -1 if none.

### 2.6 Font Subsystem (`kain_font.h`)

```c
int64_t kain_font_load(int64_t session_id, const uint8_t* ttf_data, int64_t ttf_len, float size);
int64_t kain_font_load_path(int64_t session_id, const char* filepath, float size);
int64_t kain_font_load_default(int64_t session_id, float size);
void*   kain_font_get_glyph(int64_t session_id, int64_t font_id, int codepoint);
void    kain_font_release_glyph(void* glyph);
float   kain_font_measure_text(int64_t session_id, int64_t font_id, const char* text);
float   kain_font_line_height(int64_t session_id, int64_t font_id);
KainFontMetrics kain_font_get_metrics(int64_t session_id, int64_t font_id);
```

**`kain_font_load_default`** probes (in order):
1. `KAIN_UI_FONT` environment variable (explicit override)
2. Windows: `C:/Windows/Fonts/segoeui.ttf` → `arial.ttf` → `tahoma.ttf` → `consola.ttf`
3. macOS: `/System/Library/Fonts/Helvetica.ttc` → SFNS → `/Library/Fonts/Arial.ttf`
4. Linux: `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf`

### 2.7 Host Interface (`kain_host.h`)

Platform-agnostic host vtable. Each platform backend (Win32 GDI, Vulkan, X11, Wayland, macOS, WASM) implements this.

```c
typedef struct kainHostVTable {
    const char* (*backend_id)(void);
    kainHostPlatform (*platform)(void);
    void*   (*window_create)(const char* title, int width, int height);
    void    (*window_destroy)(void* state);
    void    (*window_set_title)(void* state, const char* title);
    void    (*window_set_size)(void* state, int width, int height);
    void    (*window_get_size)(void* state, int* out_w, int* out_h);
    float   (*window_get_dpi)(void* state);
    void    (*pump_events)(void* state);
    int     (*should_close)(void* state);
    uint32_t* (*get_framebuffer)(void* state, int* out_stride_elems);
    void    (*present)(void* state, void* session);
    int     (*clipboard_set_text)(void* state, const char* text);
    int     (*clipboard_get_text)(void* state, char* out, size_t cap);
    void    (*set_cursor)(void* state, kainHostCursor cursor);
    void*   (*get_gpu_surface)(void* state);
} kainHostVTable;
```

**Platforms:** `KAIN_HOST_WIN32` (implemented), `KAIN_HOST_X11`, `KAIN_HOST_WAYLAND`, `KAIN_HOST_MACOS`, `KAIN_HOST_WASM` (future).

### 2.8 Existing Runtime Files (preserved)

The C substrate is an **addition, not a replacement**. The existing runtime files are preserved unchanged for backward compatibility:

| File | Lines | Preserved? |
|------|-------|------------|
| `ui_system.c` | ~2600 | Yes — core session engine |
| `ui_host_adapter.c` | ~520 | Yes — `abi_ui_*` ABI exports must stay |
| `ui_renderer.c` | ~350 | Yes — internal dispatch now calls `kain_render_*` primitives |
| `ui_layout.c` | ~220 | Preserved (will be deleted in Phase 5) |
| `ui_color.c` | ~220 | Yes |
| `ui_runtime.c` | ~1000 | Yes |
| `ui_compiled_bundle.c` | ~610 | Yes |
| `ui_hot_reload.c` | ~650 | Yes |
| `widgets/ui_widget.c` | ~1559 | Preserved (will be deleted in Phase 5) |
| `widgets/ui_widget.h` | ~273 | Preserved (will be deleted in Phase 5) |
| `native_ui_surface.c` | ~280 | Yes — expanded with 5 new vtable slot wrappers |

---

## 3. The KainComponentSurface Vtable (Layer 2)

### 3.1 The ABI Contract

The `KainComponentSurface` struct in `X:/runtime/native/include/component_surface.h` is **the** ABI contract between the Kain compiler and any rendering backend. It is a trait-style vtable with **24 function pointer slots**. The compiler emits calls through this vtable; backends implement them.

```c
typedef struct KainComponentSurface {
    // ── Session lifecycle (slots 0-1) ──
    int64_t (*session_create)(const char* name, int64_t width, int64_t height);
    void    (*session_destroy)(int64_t session_id);

    // ── Element tree (slots 2-4) ──
    int64_t (*element_begin)(int64_t session_id, int64_t parent_id,
                             const char* kind, const char* stable_key);
    void    (*element_end)(int64_t session_id, int64_t element_id);
    void    (*element_set_text)(int64_t session_id, int64_t element_id, const char* text);

    // ── Style/attribute setters (slots 5-7) ──
    void    (*element_set_attr_i64)(int64_t session_id, int64_t element_id,
                                     const char* key, int64_t value);
    void    (*element_set_attr_f64)(int64_t session_id, int64_t element_id,
                                     const char* key, double value);
    void    (*element_set_attr_string)(int64_t session_id, int64_t element_id,
                                        const char* key, const char* value);

    // ── State persistence i64 (slots 8-9) ──
    int64_t (*state_get_i64)(int64_t session_id, const char* key);
    void    (*state_set_i64)(int64_t session_id, const char* key, int64_t value);

    // ── Frame lifecycle (slots 10-12) ──
    void    (*begin_frame)(int64_t session_id, double delta_ms);
    void    (*end_frame)(int64_t session_id);
    void    (*present)(int64_t session_id);

    // ── Events (slots 13-14) ──
    int64_t (*poll_event)(int64_t session_id, void* out_event, int64_t max_size);
    int64_t (*should_close)(int64_t session_id);

    // ── Window lifecycle (slots 15-16) ──
    int64_t (*window_open)(int64_t session_id, const char* title,
                           int64_t width, int64_t height);
    int64_t (*host_pump)(int64_t session_id);

    // ── Platform handle (slot 17) ──
    void    (*session_attach_platform)(int64_t session_id, void* platform_handle);

    // ── GPU extension (slot 18) ──
    const KainGpuSurfaceExtension* (*get_gpu_extension)(int64_t session_id);

    // ── Expanded state persistence (slots 19-22) ──
    double      (*state_get_f64)(int64_t session_id, const char* key);        // slot 19
    void        (*state_set_f64)(int64_t session_id, const char* key, double value);  // slot 20
    const char* (*state_get_string)(int64_t session_id, const char* key);     // slot 21
    void        (*state_set_string)(int64_t session_id, const char* key, const char* value); // slot 22

    // ── Callback binding (slot 23) ──
    void    (*element_set_callback)(int64_t session_id, int64_t element_id,
                                     const char* event_name, void* callback_fn);
} KainComponentSurface;
```

**Vtable size:** 24 * 8 = 192 bytes on x64.

### 3.2 Slot Reference

| Slot | Name | Category | Purpose |
|------|------|----------|---------|
| 0 | `session_create` | Session | Create UI session, returns session_id |
| 1 | `session_destroy` | Session | Destroy session |
| 2 | `element_begin` | Element | Start a named element in the retained tree |
| 3 | `element_end` | Element | Close an element |
| 4 | `element_set_text` | Element | Set text content on element |
| 5 | `element_set_attr_i64` | Style | Set integer attribute (direction, disabled, etc.) |
| 6 | `element_set_attr_f64` | Style | Set float attribute (padding, spacing, font_size, etc.) |
| 7 | `element_set_attr_string` | Style | Set string attribute (fill_color, border_color, etc.) |
| 8 | `state_get_i64` | State | Get integer state (counter, flag) |
| 9 | `state_set_i64` | State | Set integer state |
| 10 | `begin_frame` | Frame | Start frame, reset per-frame arena |
| 11 | `end_frame` | Frame | End frame, signal completion |
| 12 | `present` | Frame | Present framebuffer to screen |
| 13 | `poll_event` | Events | Poll for next event (non-blocking) |
| 14 | `should_close` | Events | Window close requested? |
| 15 | `window_open` | Window | Create platform window |
| 16 | `host_pump` | Window | Pump platform message loop |
| 17 | `session_attach_platform` | Platform | Attach platform native handle |
| 18 | `get_gpu_extension` | GPU | Get GPU surface extension (or NULL) |
| 19 | `state_get_f64` | State | Get float state (slider value, animation progress) |
| 20 | `state_set_f64` | State | Set float state |
| 21 | `state_get_string` | State | Get string state (textbox content) |
| 22 | `state_set_string` | State | Set string state |
| 23 | `element_set_callback` | Events | Bind callback fn pointer to named event on element |

### 3.3 Vtable Invariants

- **APPEND ONLY.** Slots are never inserted, reordered, or deleted. The compiler's `OFF_*` constants in `crates/sys-codegen/src/codegen_llvm/component.rs` must match C struct field order exactly.
- The `native_ui_surface` backend (GDI/software) registers at startup. GPU backends (vulkan, d3d12, webgpu) register via their respective shim files.
- `kain_component_surface_resolve("native_ui")` returns the backend vtable pointer for codegen to cache.

### 3.4 How the Compiler Emits Vtable Calls

The Rust codegen in `crates/sys-codegen/src/codegen_llvm/component.rs` maps JSX to vtable calls:

```
<box fill_color="accent" width={100} height={30}>
  <text value="Click Me" font_size={14} />
</box>
```

becomes (pseudocode):

```
// element_begin: <box>
let box_id = surface->element_begin(session, parent, "box", "box_0")
surface->element_set_attr_string(session, box_id, "fill_color", "accent")
surface->element_set_attr_f64(session, box_id, "width", 100.0)
surface->element_set_attr_f64(session, box_id, "height", 30.0)

// element_begin: <text>
let text_id = surface->element_begin(session, box_id, "text", "text_0")
surface->element_set_text(session, text_id, "Click Me")
surface->element_set_attr_f64(session, text_id, "font_size", 14.0)
surface->element_end(session, text_id)

surface->element_end(session, box_id)
```

### 3.5 JSX Attribute → Vtable Slot Mapping

| JSX Attribute | Vtable Slot | Style Key |
|---------------|------------|-----------|
| `width` | `element_set_attr_f64` (6) | `"width"` |
| `height` | `element_set_attr_f64` (6) | `"height"` |
| `padding` | `element_set_attr_f64` (6) | `"padding"` |
| `spacing` | `element_set_attr_f64` (6) | `"spacing"` |
| `corner_radius` | `element_set_attr_f64` (6) | `"corner_radius"` |
| `font_size` | `element_set_attr_f64` (6) | `"font_size"` |
| `opacity` | `element_set_attr_f64` (6) | `"opacity"` |
| `border_width` | `element_set_attr_f64` (6) | `"border_width"` |
| `fill_color` | `element_set_attr_string` (7) | `"fill_color"` |
| `background` | `element_set_attr_string` (7) | `"fill_color"` |
| `border_color` | `element_set_attr_string` (7) | `"border_color"` |
| `color` | `element_set_attr_string` (7) | `"ink_color"` |
| `title` | `element_set_attr_string` (7) | `"title"` |
| `value` | `element_set_text` (4) | (text content) |
| `direction` | `element_set_attr_i64` (5) | `"layout.direction"` |
| `disabled` | `element_set_attr_i64` (5) | `"disabled"` |
| `on_click` | `element_set_callback` (23) | event binding |

---

## 4. The Kain Widget Library (Layer 3)

The stdlib widget library lives in `X:/stdlib/ui/` — **26 `.kn` files** organized into primitives, layout, and components. The re-export hub is `X:/stdlib/ui.kn`.

### 4.1 Directory Structure

```text
stdlib/ui/
├── ui.kn                      ← Top-level module (re-export hub + backward-compat @extern)
├── core.kn                    ← ALL @extern ABI declarations (~135 lines)
├── theme.kn                   ← Color, Spacing, Theme, DEFAULT_THEME structs (~215 lines)
├── style.kn                   ← DEPRECATED — color constants, shim to theme.kn
├── component.kn               ← DEPRECATED — bridge (was 158 lines, now shim)
├── widget.kn                  ← DEPRECATED — immediate-mode wrappers (now shim)
├── font.kn                    ← Font helpers
│
├── primitives/                ← L0 rendering primitives (6 files)
│   ├── rect.kn                ← RoundedRect
│   ├── circle.kn              ← Circle
│   ├── text.kn                ← Text
│   ├── interactive.kn         ← InteractiveArea (hover/press/click/drag)
│   ├── image.kn               ← Image (future)
│   └── gradient.kn            ← GradientRect (future)
│
├── layout/                    ← Layout containers (6 files)
│   ├── stack.kn               ← HStack + VStack + ZStack
│   ├── grid.kn                ← Grid
│   ├── spacer.kn              ← Spacer
│   ├── padding.kn             ← Padding
│   ├── scroll.kn              ← ScrollView
│   └── divider.kn             ← Divider
│
└── components/                ← Interactive widgets (8 files)
    ├── label.kn               ← Label
    ├── button.kn              ← Button (hover/press states)
    ├── textinput.kn           ← TextInput (focused, cursor, password)
    ├── checkbox.kn            ← Checkbox (checked, hovered)
    ├── slider.kn              ← Slider (draggable thumb, step)
    ├── toggle.kn              ← Toggle (capsule switch, animated thumb)
    ├── progress.kn            ← ProgressBar (value/max, variants)
    └── spinner.kn             ← Spinner (animated rotation)
```

### 4.2 Primitives

Stateless rendering components that emit draw commands through the C renderer.

#### RoundedRect

```kain
component RoundedRect(
    width: Float, height: Float, corner_radius: Float,
    fill_color: String, stroke_color: String, stroke_width: Float, opacity: Float,
):
```

Renders a filled, optionally rounded rectangle via the `<box>` JSX element.

#### Circle

```kain
component Circle(
    radius: Float, fill_color: String, stroke_color: String, stroke_width: Float,
):
```

Renders a filled or stroked circle.

#### Text

```kain
component Text(
    value: String, font_size: Float, color: String,
    align: String, weight: Int, font_family: String,
):
```

Renders text via the `<text>` JSX element.

#### InteractiveArea

```kain
component InteractiveArea(
    width: Float, height: Float, cursor: String, enabled: Bool,
):
    state hovered: Bool = false
    state pressed: Bool = false
```

Invisible hit-testable region with pointer event tracking. Tracks `hovered` and `pressed` state.

#### Image (stub)

```kain
component Image(src: String, width: Float, height: Float, fit: String, alt: String):
```

#### GradientRect (stub)

```kain
component GradientRect(
    colors: String, stops: String, direction: String,
):
```

### 4.3 Layout

#### HStack -- Horizontal Stack

```kain
component HStack(
    spacing: Float, alignment: String, distribution: String,
    width: Float, height: Float,
):
```

Arranges children left-to-right.

**Alignment:** `"start"`, `"center"`, `"end"`, `"fill"`

**Distribution:** `"start"`, `"center"`, `"end"`, `"space_between"`, `"space_around"`, `"space_evenly"`

#### VStack -- Vertical Stack

```kain
component VStack(
    spacing: Float, alignment: String, distribution: String,
    width: Float, height: Float,
):
```

Arranges children top-to-bottom. Same alignment/distribution as HStack.

#### ZStack -- Z-Axis Stack

```kain
component ZStack(
    alignment: String, width: Float, height: Float,
):
```

Stacks children back-to-front, all at the same position. **Alignment:** `"top_left"`, `"center"`, `"top_right"`, `"bottom_left"`, `"bottom_right"`.

#### Grid

```kain
component Grid(
    columns: Int, spacing: Float, width: Float, height: Float,
):
```

Arranges children in row-major order across a column grid. Cell dimensions computed from available width/height.

#### Spacer

```kain
component Spacer(min_size: Float, axis: String):
```

Flexible space that expands to fill available room. `min_size=0` = fully flexible; `min_size>0` = fixed gap.

#### Padding

```kain
component Padding(
    all: Float, horizontal: Float, vertical: Float,
    left: Float, top: Float, right: Float, bottom: Float,
    width: Float, height: Float,
):
```

Insets children by specified padding. Resolution priority: explicit per-side > `all` > `horizontal`/`vertical` > 0.

#### ScrollView

```kain
component ScrollView(
    axis: String, show_indicator: Bool, width: Float, height: Float,
) with Reactive:
    state scroll_x: Float = 0.0
    state scroll_y: Float = 0.0
    state content_width: Float = 0.0
    state content_height: Float = 0.0
```

Scrollable viewport. Supports vertical, horizontal, or both axes. Content rendered at offset `(-scroll_x, -scroll_y)`.

#### Divider

```kain
component Divider(
    orientation: String, color: String, thickness: Float,
    margin: Float, width: Float, height: Float,
):
```

Visual separator line. `"horizontal"` or `"vertical"`.

### 4.4 Interactive Components

#### Label

```kain
component Label(
    value: String, font_size: Float, color: String,
    align: String, weight: Int, width: Float, height: Float,
):
```

Thin wrapper over Text primitive with theme defaults (body font size 14, color `"text"`, left-aligned).

#### Button

```kain
component Button(
    label: String, variant: String, disabled: Bool,
    width: Float, height: Float,
):
    state hovered: Bool = false
    state pressed: Bool = false
```

Interactive button with hover/press visual states.

**Variants:** `"primary"` (accent), `"secondary"` (accent2), `"destructive"` (accent4), `"ghost"` (transparent).

Default size: 100x30. Color selection:
- disabled → `"button_disabled"`
- pressed → `"button_press"`
- hovered → `"button_hover"`
- primary → `"accent"`, secondary → `"accent2"`, destructive → `"accent4"`, ghost → `"transparent"`

#### TextInput

```kain
component TextInput(
    value: String, placeholder: String, width: Float, password: Bool,
):
    state focused: Bool = false
    state cursor_pos: Int = len(value)
    state cursor_blink: Bool = true
```

Single-line editable text field. Password mode masks input with `\u25CF` (black circle). Blinking cursor when focused. Default width: 200.

#### Checkbox

```kain
component Checkbox(
    label: String, checked: Bool,
):
    state hovered: Bool = false
```

Square toggle box (18x18) with checkmark (`✓`) and optional label. Checked → accent fill; hovered → button_hover fill.

#### Slider

```kain
component Slider(
    value: Float, min: Float, max: Float, step: Float,
):
    state dragging: Bool = false
    state drag_value: Float = value
```

Horizontal track (200x20) with draggable thumb (16x16). Value clamped to `[min, max]`. Step granularity snaps the thumb to discrete positions.

#### Toggle

```kain
component Toggle(
    checked: Bool, disabled: Bool,
):
    state hovered: Bool = false
    state anim_progress: Float = if checked: 1.0 else: 0.0
```

Capsule-shaped toggle switch (44x24). Thumb slides between on (right) and off (left) positions.

#### ProgressBar

```kain
component ProgressBar(
    value: Float, max: Float, variant: String,
):
```

Horizontal bar (200x18) showing completion ratio. **Variants:** `"success"` (accent/green), `"warning"` (accent3/orange), `"error"` (accent4/red), default (accent2/blue). Shows percentage label.

#### Spinner

```kain
component Spinner(
    size: Float, color: String, speed: Float,
):
    state angle: Float = 0.0
```

Animated loading spinner (default 24x24). Pulse-driven rotation. Color defaults to `"accent"`.

---

## 5. The Theme System

### 5.1 Location

`X:/stdlib/ui/theme.kn` — Pure Kain, no C dependency. Replaces the 19 `#define` colors in `ui_widget.h`.

### 5.2 Color Struct

```kain
pub struct Color:
    r: Float  // 0.0 - 1.0
    g: Float
    b: Float
    a: Float

pub fn color_rgba(r: Float, g: Float, b: Float, a: Float) -> Color:
pub fn color_rgb(r: Float, g: Float, b: Float) -> Color:
pub fn color_pack(c: Color) -> Int:             // → 0xAARRGGBB
pub fn color_from_packed(packed: Int) -> Color: // ← 0xAARRGGBB
pub fn color_lerp(a: Color, b: Color, t: Float) -> Color:
pub fn color_with_alpha(c: Color, alpha: Float) -> Color:
pub fn color_blend(over: Color, under: Color) -> Color:
pub fn color_to_renderer_format(val: Int) -> Int:  // ARGB → ABGR
```

### 5.3 Spacing Struct

```kain
pub struct Spacing:
    xs: Float    // 2px
    sm: Float    // 4px
    md: Float    // 8px
    lg: Float    // 16px
    xl: Float    // 24px
    xxl: Float   // 32px
    section: Float // 48px

pub const SPACING: Spacing = Spacing {
    xs: 2.0, sm: 4.0, md: 8.0, lg: 16.0,
    xl: 24.0, xxl: 32.0, section: 48.0,
}
```

### 5.4 Theme Struct

```kain
pub struct Theme:
    bg: Color              // #1A1A24 — deepest background
    surface: Color         // #252540 — panels
    surface2: Color        // #2E2E48 — elevated surfaces
    header: Color          // #1E1E32 — title bars
    accent: Color          // #21D4A1 — primary interactive (teal)
    accent2: Color         // #4A90D9 — secondary (blue)
    accent3: Color         // #E8914A — tertiary (orange)
    accent4: Color         // #E84A5F — destructive/warning (red)
    text: Color            // #E8E8F0 — primary text
    text_dim: Color        // #8888A0 — muted text
    text_inverse: Color    // — text on accent backgrounds
    border: Color          // #3A3A5C — borders
    highlight: Color       // #2A2A4E — selection highlight
    button: Color          // #303050 — normal button
    button_hover: Color    // #404068 — button hover
    button_press: Color    // #505080 — button pressed
    button_disabled: Color // — disabled button
    input_bg: Color        // #0A0A14 — text input background
    input_focus: Color     // — focused input
    slider_track: Color    // #3A3A5C
    slider_fill: Color     // #2A2A44
    slider_thumb: Color    // #21D4A1
    spacing: Spacing
    corner_radius: CornerRadius
    font_size: Float       // 14.0
    border_width: Float    // 1.0
    shadow_opacity: Float  // 0.3
```

### 5.5 DEFAULT_THEME

```kain
pub const DEFAULT_THEME: Theme = Theme {
    bg:      Color { r: 0.102, g: 0.102, b: 0.141, a: 1.0 },  // #1A1A24
    surface: Color { r: 0.145, g: 0.145, b: 0.251, a: 1.0 },  // #252540
    accent:  Color { r: 0.129, g: 0.831, b: 0.631, a: 1.0 },  // #21D4A1
    text:    Color { r: 0.910, g: 0.910, b: 0.941, a: 1.0 },  // #E8E8F0
    // ... all 27 slots populated ...
    spacing: SPACING,
    corner_radius: CornerRadius {
        top_left: 4.0, top_right: 4.0,
        bottom_right: 4.0, bottom_left: 4.0,
    },
    font_size: 14.0,
    border_width: 1.0,
    shadow_opacity: 0.3,
}
```

---

## 6. The `component` Keyword

### 6.1 Component Anatomy

```kain
component Button(
    label: String,       // Props — typed, optional defaults
    variant: String,
    disabled: Bool,
):
    state hovered: Bool = false   // Persistent state (survives frames)
    state pressed: Bool = false

    fn compute_fill(_self: Self_) -> String:  // Methods — _self: Self_ always first
        if _self.disabled: return "button_disabled"
        if _self.pressed:  return "button_press"
        if _self.hovered:  return "button_hover"
        return "button"

    render <box fill_color={compute_fill()}>
        <text value={label} font_size={14.0} />
    </box>
```

### 6.2 State Persistence

All three state types are supported via vtable slots:

| State Type | Vtable Slots | Example |
|-----------|-------------|---------|
| `Bool` / `Int` / `i64` | 8-9 (`state_get_i64` / `state_set_i64`) | Counter, flags, hovered, pressed |
| `Float` / `f64` | 19-20 (`state_get_f64` / `state_set_f64`) | Slider value, opacity, animation progress |
| `String` | 21-22 (`state_get_string` / `state_set_string`) | TextInput content |

### 6.3 JSX Elements

The compiler maps JSX tag names to vtable `element_begin` calls:

| JSX Tag | Vtable `kind` | Purpose |
|---------|-------------|---------|
| `<box>` | `"box"` | Filled rectangle with corner_radius, fill_color, stroke |
| `<text>` | `"text"` | Text content with font_size, color, align |
| `<stack>` | `"stack"` | Layout container with direction (horizontal/vertical/z), gap, alignment |
| `<ComponentName>` | `"ComponentName"` | Nested component call |

### 6.4 JSX Attribute → Vtable Mapping (Complete)

See [Section 3.5](#35-jsx-attribute--vtable-slot-mapping) for the full mapping table.

### 6.5 JSX Control Flow

```kain
// For loop
render <VStack>
    for item in items:
        <Label value={item.name} />
</VStack>

// Conditional
render <box>
    if _self.selected:
        <text value="Selected" color="accent" />
    else:
        <text value="Not selected" color="text_dim" />
</box>

// Expression interpolation
render <text value={"Hello " + name} font_size={14.0} />
```

### 6.6 Pulse & Resonate in Components (Phase 3)

Components can embed `pulse` and `resonate` blocks:

```kain
component AnimatedWidget:
    state angle: Float = 0.0

    pulse animation_clock every 16ms jitter 0ms:
        self.angle = self.angle + 0.05

    // resonate on a world field
    resonate AppState.theme dampen 0ms:
        self.current_theme = resonate_new_string
```

---

## 7. The Ghost Harness

### 7.1 Location

`X:/blades/ui_demos/harness.py` — Python harness for gaslight-proof UI testing.

### 7.2 How It Works

The harness launches Kain apps in an **invisible, transparent window** (alpha=1/255, click-through, hidden from Alt-Tab). It uses Win32 `PrintWindow(PW_RENDERFULLCONTENT)` to capture the raw render buffer before DWM compositing. The GPU renders at full speed; the user sees nothing.

The captured frame is fed to an LLM (LM Studio, `google/gemma-4-e2b` by default) for visual analysis.

### 7.3 Usage

```bash
# Build + ghost-capture + analyze a .kn file
python harness.py <kain_file.kn>

# Ghost-capture a pre-built .exe
python harness.py --exe <path.exe>

# Batch analyze all .kn files in a folder
python harness.py --folder <dir>

# Ghost-capture an already-running process
python harness.py --pid <pid>
```

### 7.4 Integration

The ghost harness integrates with the Oracle tool (`oracle scan → launch → debug → matrix → verify → delta`) for end-to-end UI validation. Every PR that changes the renderer should pass the ghost harness.

### 7.5 LLM Analysis Output

The harness sends captured frames to an LLM for analysis with this rubric:
1. **One-word verdict:** BLANK, CRASHED, or RENDERING
2. Visual breakdown — exact hex colors, layout structure, visible elements
3. Hardcoding assessment — theme-driven vs. arbitrary colors/sizes
4. SwiftUI comparison — frame quality vs. Apple's framework

---

## 8. Taxonomy

### 8.1 C Demos (`runtime/native/src/ui/test_ui_v2/`)

6 next-gen C demos used as regression targets for the C substrate:

| Demo | Lines | What It Does |
|------|-------|-------------|
| `cosmic_dashboard.c` | ~1763 | NASA/JPL mission control: 350 particles, nebula gradient, 6 glass panels, 8 fonts |
| `retrowave.c` | ~1540 | Synthwave spectacle: scrolling grid, neon sunset, 3D wireframe, 5 glowing panels |
| `ui3d_sandbox.c` | ~1330 | 3D rendering: rotation matrices, depth sorting, isometric grid, particle fountain |
| `font_inferno.c` | ~1270 | Font stress test: 14+ fonts, all sizes, color cycling |
| `fire_life_plasma.c` | ~1130 | Cellular automata: fire effect, Conway's Game of Life, plasma fractal |
| `voxel_viewer.c` | ~1450 | Isometric voxel terrain viewer |

### 8.2 Kain UI Demos (`blades/ui_demos/`)

| Demo | What It Shows |
|------|---------------|
| `widget_showcase.kn` | All 8 widgets, layout, fonts, state management |
| `retrowave_lite.kn` | Framebuffer pixel art, stars, grid, equalizer, slider |
| `font_gallery.kn` | 14 fonts loaded, sample text rendering, color cycling |

### 8.3 Test Folder Structure

```
runtime/native/src/ui/
├── test_ui/        ← 10+ C demos (calculator, keypad, widget_demo, full_demo, etc.)
├── test_ui_v2/     ← 6 next-gen C demos (regression targets)
├── debug_ui/       ← 7 debug/minimal C tests
└── fuzz/           ← Fuzzer harness (renderer API fuzzing)

blades/
├── ui_demos/       ← Kain-authored UI demos
├── window_proof/   ← 17-line minimal window, Oracle-verified
└── cosmic_dashboard/ ← Kain port of cosmic dashboard (WIP)
```

---

## 9. Building & Running

### 9.1 C Runtime (Makefile)

```bash
cd X:\runtime\native\src\ui

make              # Build everything: libkain_ui.a + tests + demos
make static       # Build libkain_ui.a only (static library)
make libs         # Build .o files only (incremental)
make tests        # Build test_ui/ executables
make demos        # Build test_ui_v2/ demo executables
make widgets      # Build widget library test
make -j8          # Parallel build (8 jobs)
make clean        # Remove all artifacts

# Run demos
make run_cosmic   # Build + run cosmic dashboard
make run_retro    # Build + run retro wave
make run_3d       # Build + run 3D sandbox
make run_flp      # Build + run fire+life+plasma
make run_voxel    # Build + run voxel viewer

# Fuzz targets
make fuzz         # Build fuzzer binary
make fuzz-run     # 10k iterations
make fuzz-stress  # 500k iterations
```

### 9.2 Source Files in Build

The Makefile builds 5 source groups:

| Group | Files |
|-------|-------|
| UI core | `ui_system.c`, `ui_host_adapter.c`, `ui_renderer.c`, `ui_layout.c`, `ui_color.c`, `ui_hot_reload.c`, `ui_runtime.c`, `ui_compiled_bundle.c`, `native_ui_surface.c` |
| Core | `component_surface.c`, `input_system.c` |
| Kain substrate | `kain_render_software.c`, `kain_compositor.c`, `kain_input.c`, `kain_font.c`, `kain_host_win32.c` |
| Widgets | `ui_widget.c` |

### 9.3 Kain Applications

```bash
# Typecheck
kain check my_ui_app.kn

# Build to native executable
kain build my_ui_app.kn --target llvm

# Run directly
kain run my_ui_app.kn --target llvm

# Run Kain demos
kain run blades/ui_demos/widget_showcase.kn --target llvm
kain run blades/ui_demos/retrowave_lite.kn --target llvm
kain run blades/ui_demos/font_gallery.kn --target llvm
```

### 9.4 Bazel Build

```bash
# Build the compiler
bazel build //:kain --config=dev

# Build the native runtime
bazel build //runtime:native_core_runtime --config=dev

# Sync to ~/.kain/bin/
kain_sync_binary
```

### 9.5 Runtime Sync

After any Bazel build, sync the native runtime:

```bash
kain_sync_binary     # Syncs both compiler + runtime
kain_status          # Check freshness
```

The `update_runtime.py` script in `scripts/python/` handles copying the built runtime library.

---

## 10. Quick Reference

### 10.1 Common Imports

```kain
use std::ui                    // Core UI ABI (session, nodes, events, rendering)
use std::ui::core              // @extern ABI bridge (preferred for new code)
use std::ui::theme             // Color, Spacing, Theme, DEFAULT_THEME
use std::ui::components::*     // Button, Label, TextInput, Checkbox, Slider, Toggle, ProgressBar, Spinner
use std::ui::layout::*         // HStack, VStack, ZStack, Grid, Spacer, Padding, ScrollView, Divider
use std::ui::primitives::*     // RoundedRect, Circle, Text, InteractiveArea, Image, GradientRect
use std::ui::style             // DEPRECATED — backward-compatible color constants
use std::ui::component         // DEPRECATED — widget bridge shim
use std::ui::widget            // DEPRECATED — immediate-mode widget shim
use std::input                 // Input system (keyboard, mouse, bindings)
use std::graphics              // GPU graphics (buffers, shaders, pipelines)
```

### 10.2 The Canonical Frame Loop

```kain
use std::ui
use std::ui::core
use std::ui::components::Button

pub fn main() -> Int:
    // 1. Create session + window + attach host
    let session = ui_host_session_create(
        "MyApp", "My Kain App", 800, 600, "winit")

    // 2. Load default font
    let _font = native_ui_font_create(session, "default", "Segoe UI", 14.0)

    // 3. Frame loop
    while native_ui_host_should_close(session) == 0:
        let _pump = native_ui_host_pump(session)
        let _bf = native_ui_begin_frame(session, 16.0)

        // ── YOUR UI HERE ──
        // Components render through the vtable

        let _fe = native_ui_end_frame(session)
        let _pr = native_ui_present(session)

    // 4. Cleanup
    let _ds = native_ui_session_destroy(session)
    return 0
```

### 10.3 Convenience: `ui_host_session_create`

```kain
pub fn ui_host_session_create(
    app_name: String, window_title: String,
    width: Int, height: Int, backend_id: String,
) -> Int:
    let session = native_ui_session_create(app_name, width, height)
    let _window = native_ui_window_open(session, window_title, width, height)
    let _host   = native_ui_host_attach(session, backend_id)
    return session
```

### 10.4 `ui.kn` Quick Ref — All 83+ ABI Functions

All ABI functions are declared in `x:/stdlib/ui.kn` with `@extern`:

| Category | Key Functions |
|----------|--------------|
| **Session** | `native_ui_reset`, `native_ui_session_create/destroy/count`, `native_ui_window_open/close/invalidate_window` |
| **Frame** | `native_ui_begin_frame/end_frame/present`, `native_ui_frame_index`, `native_ui_mark_dirty`, `native_ui_dirty_count` |
| **Nodes** | `native_ui_node_create/destroy`, `native_ui_node_set_rect/parent`, `native_ui_node_set_stable_key`, `native_ui_node_find_by_stable_key`, `native_ui_node_child_count`, `native_ui_reconcile_node` |
| **Style** | `ui_style_color_rgba/rgb`, `ui_style_padding`, `ui_style_string/set_string` |
| **State** | `ui_state_set_bool/f64/string`, `ui_state_counter`, `ui_state_toggle`, `ui_state_shared_buffer_resource`, `ui_state_shared_image_resource` |
| **Draw** | `native_ui_draw_rect/text/resource/gradient` |
| **Events** | `native_ui_poll_event`, `native_ui_event_kind/target/x/y/key_code/text`, `native_ui_push_event`, `native_ui_hit_test` |
| **Fonts** | `native_ui_font_create/destroy`, `native_ui_font_get_glyph`, `native_ui_text_measure_width/height` |
| **Host** | `native_ui_host_attach/detach`, `native_ui_host_pump/should_close`, `native_ui_host_clipboard_get/set`, `native_ui_host_dpi`, `ui_fb_ptr/width/height/stride` |
| **Resources** | `native_ui_resource_create_type`, `native_ui_resource_set_bytes_hex`, `native_ui_resource_type/key/byte_length`, `native_ui_shader_create` |
| **Focus** | `native_ui_focus`, `native_ui_focused_node`, `native_ui_focus_next/prev`, `native_ui_blur_node` |

### 10.5 Key Constants (Backward Compatible in `std::ui::style`)

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

**Note:** New code should use `std::ui::theme` instead of `std::ui::style`.

### 10.6 Named Color Keys (for JSX `fill_color` / `color` attrs)

Components use **named color keys** that the renderer resolves through its own style tables:

| Key | Theme Field | Purpose |
|-----|-----------|---------|
| `"bg"` | `theme.bg` | Deepest background |
| `"surface"` | `theme.surface` | Panel backgrounds |
| `"accent"` | `theme.accent` | Primary interactive |
| `"accent2"` | `theme.accent2` | Secondary accent |
| `"accent3"` | `theme.accent3` | Tertiary accent |
| `"accent4"` | `theme.accent4` | Destructive/warning |
| `"text"` | `theme.text` | Primary text |
| `"text_dim"` | `theme.text_dim` | Muted text |
| `"text_inverse"` | `theme.text_inverse` | Text on accent |
| `"border"` | `theme.border` | Borders/lines |
| `"button"` | `theme.button` | Button normal |
| `"button_hover"` | `theme.button_hover` | Button hovered |
| `"button_press"` | `theme.button_press` | Button pressed |
| `"button_disabled"` | `theme.button_disabled` | Button disabled |
| `"input_bg"` | `theme.input_bg` | Text input |
| `"slider_track"` | `theme.slider_track` | Slider track |
| `"slider_fill"` | `theme.slider_fill` | Slider fill |
| `"slider_thumb"` | `theme.slider_thumb` | Slider thumb |
| `"transparent"` | — | Transparent |

---

## 11. GPU Rendering (Future Path)

### 11.1 Architecture

The `KainComponentSurface` vtable is backend-agnostic. GPU backends register their own vtable implementations:

```
Backend Registration:
  vulkan_surface_shim.c   → registers "vulkan"   (323 lines)
  d3d12_surface_shim.c    → registers "d3d12"    (284 lines)
  webgpu_surface_shim.c   → registers "webgpu"   (319 lines)
  native_ui_surface.c     → registers "native_ui" (software GDI)
```

GPU backends implement all 24 vtable slots and return a `KainGpuSurfaceExtension` from slot 18.

### 11.2 Backend Selection

```kain
// Software (default, always works)
let session = ui_host_session_create("App", "My App", 800, 600, "winit")

// GPU (when available)
let session = ui_host_session_create("App", "My App", 800, 600, "vulkan")
```

### 11.3 GPU Surface Extension

```c
typedef struct KainGpuSurfaceExtension {
    int64_t (*load_shader)(int64_t session_id, const char* spirv_hex);
    int64_t (*set_uniform)(int64_t session_id, uint32_t binding,
                            const void* data, uint64_t size);
} KainGpuSurfaceExtension;
```

Software backends return `NULL` for `get_gpu_extension()`. GPU backends return a fully populated extension.

### 11.4 Current GPU Backend Status

| Component | Status |
|-----------|--------|
| `KainComponentSurface` vtable (24 slots) | **Live** |
| Vulkan ABI loader (`vulkan_abi.c`, ~2050 lines, 43 PFNs) | **Built** |
| Vulkan surface shim (`vulkan_surface_shim.c`) | **Built** |
| D3D12 surface shim (`d3d12_surface_shim.c`) | **Built** |
| WebGPU surface shim (`webgpu_surface_shim.c`) | **Built** |
| GPU runtime library (`kain_gpu_runtime.dll`) | **Cataloged** |
| `std::graphics` module (Kain bindings) | **Live** |
| Software DIB renderer | **Primary path** |

---

## 12. Migration Status & Deprecation

### 12.1 Phase Summary

| Phase | Status | Description |
|-------|--------|-------------|
| **1: C Substrate** | **COMPLETE** | 7 new files extracted from monolith; demos render identically |
| **2: GPU Backends** | Future | Vulkan + WebGPU renderer behind `kain_render_*` API |
| **3: Compiler Pipeline** | Future | JSX attr expansion, pulse/resonate in components, f64/String state, callbacks |
| **4: Kain Widget Library** | **COMPLETE** | 26 `.kn` files — all primitives, layouts, and components |
| **5: Delete C Widgets** | Future | Remove `ui_widget.c/h`, `ui_layout.c`, `draw_commands[]` ring buffer |
| **6: Animation** | Future | Easing curves, SpringValue, Transition, pulse-driven animation |
| **7: Hot Reload** | Future | Component-level render fn pointer swap |
| **8: Accessibility** | Future | World-graph → UIA/AT-SPI/AX bridge |
| **9: Portability** | Future | X11, Wayland, macOS, WASM host backends |
| **10: Advanced** | Future | Cross-process UI via teleport, GPU compute in components, Z3 layout |

### 12.2 Deprecated Modules

| Module | Status | Replacement |
|--------|--------|-------------|
| `std::ui::style` | Deprecated | `std::ui::theme` |
| `std::ui::component` | Deprecated shim | `std::ui::components::*` |
| `std::ui::widget` | Deprecated shim | `std::ui::components::*` + `std::ui::layout::*` |
| C `ui_widget.c` (1559 loc) | Preserved (Phase 5 delete) | 26 `.kn` component files |
| C `ui_layout.c` (199 loc) | Preserved (Phase 5 delete) | `HStack`/`VStack`/`ZStack`/`Grid` |
| C `#define UI_COLOR_*` (19 constants) | Preserved (Phase 5 delete) | `DEFAULT_THEME` in `theme.kn` |

### 12.3 Existing Blades

Existing blades calling `widget::button()`, `widget::slider()`, etc. continue working through deprecation shims in `widget.kn` and `component.kn`. These shims wrap the old C widget ABI. They will be removed in Phase 5 after all blades are migrated.

---

## Further Reading

| File | What |
|------|------|
| `X:/docs/KAIN_BY_EXAMPLE.md` | All Kain language features with compilable snippets |
| `X:/docs/RULEBOOK.md` | Decision ladder — which construct for which problem |
| `X:/docs/COMPONENT.MD` | Full component reference (props, state, methods, JSX, limitations) |
| `X:/docs/WORLD.MD` | Authority+mirror pattern, surface projections |
| `X:/docs/PULSE.MD` | Temporal beat, jitter, animation driver |
| `X:/docs/RESONATE.MD` | Dampening, reentry guard, self-feedback rule |
| `X:/runtime/native/src/ui/research/MASTER_DOC.md` | KUIF master plan — the vision, architecture, phase plan |
| `X:/runtime/native/src/ui/research/TASKS.md` | Full task breakdown with spawn strategy |
| `X:/runtime/native/src/ui/research/RENDER-AND-UI-MAP.md` | 122-file dependency graph across 16 layers |
| `X:/runtime/native/include/component_surface.h` | 24-slot vtable — THE ABI contract |
| `X:/stdlib/ui/theme.kn` | Color, Spacing, Theme, DEFAULT_THEME |
| `X:/stdlib/ui/core.kn` | All @extern ABI declarations |
| `X:/blades/ui_demos/harness.py` | Ghost harness for gaslight-proof UI testing |
| `X:/blades/ui_demos/widget_showcase.kn` | Full widget showcase |
| `X:/benchmark/cases_v2/keyword_crucible.kn` | 108/110 keywords in context |

---

> *"The compiler owns the truth. The C substrate pushes the pixels. The components are the identity."*
>
> — KUIF Master Document, 2026-06-25
