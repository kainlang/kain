# Kain Native UI Runtime

**A retained-mode, cross-platform C11 UI system** — the C substrate for the
**Kain UI Framework (KUIF)**, built into the Kain runtime. After the Phase 1-4
KUIF implementation, the system is a clean 4-layer stack: 12 source files in
`src/ui/` (the retained-mode session engine and helpers) plus **7 substrate
modules** in `src/ui/kain/` (widget-free draw primitives, compositor, input
pipeline, font subsystem, GPU surface abstraction, and platform host vtable).
The KainComponentSurface vtable is 24 slots, sits at the ABI seam between
the Kain compiler and any rendering backend, and is the single contract
every backend (native_ui, Vulkan, D3D12, WebGPU) implements.

This document is the **definitive README for the C UI runtime**. For the
multi-phase KUIF roadmap, see `research/MASTER_DOC.md`. For the
122-file cross-language dependency map, see `research/RENDER-AND-UI-MAP.md`.
For the Kain-side authoring guide, see `X:/docs/UI.MD`.

---

## Architecture Overview

```
  KAINTANA BLADES / C DEMOS / KAIN-AUTHORED APPS
       │  (KainComponentSurface vtable — 24 slots)
       ▼
  ┌──────────────────────────────────────────────────────┐
  │  native_ui_surface.c                 (~440 lines)    │  ← Ecosystem:
  │  KainComponentSurface vtable impl (reference         │     compiler surface
  │  software/GDI backend; auto-registered as           │     adapter. Other
  │  "native_ui" via CRT constructor)                    │     backends (Vulkan,
  └──────────┬───────────────────────────────────────────┘     D3D12, WebGPU) ship
             │  abi_ui_* ABI calls                           │     alongside.
             ▼
  ┌──────────────────────────────────────────────────────┐
  │  ui_system.c          (~3,000 lines)                 │  ← Core retained-mode
  │  ui_system_internal.h   (~250 lines)                 │     session engine.
  │  ui_runtime.c         (~1,280 lines)                 │     Nodes, styles,
  │  ui_compiled_bundle.c   (~780 lines)                 │     state, events,
  │  ui_host_adapter.c      (~270 lines) ← delegates to  │     resources, IME,
  │                          kainHostVTable              │     drag-drop, menus,
  │  ui_layout.c            (~200 lines)                 │     dialogs, callback
  │  ui_color.c             (~240 lines)                 │     dispatch (slot 23).
  │  ui_hot_reload.c        (~710 lines)                 │
  │  native_ui_surface.c                                  │
  └──────────┬───────────────────────────────────────────┘
             │
             ▼
  ┌──────────────────────────────────────────────────────┐
  │  src/ui/kain/ — PHASE 1 C SUBSTRATE (~2,150 lines)   │  ← 7 widget-free
  │  ─────────────────────────                            │     modules
  │  kain_geometry.h          (~215 lines)               │
  │  kain_render_software.c   (~580 lines)               │  • 16 draw primitives
  │  kain_compositor.c        (~140 lines)               │  • 64-rect damage tracker
  │  kain_input.c             (~125 lines)               │  • Typed event pipeline
  │  kain_font.c              (~160 lines)               │  • Font load/measure/glyph
  │  kain_host.h (vtable)     (~120 lines)               │  • Platform-agnostic host
  │  kain_host_win32.c        (~670 lines)               │  • Win32 GDI implementation
  │  kain_surface.h           (~60 lines)                │  • GPU surface abstraction
  │  (twin headers in include/kain_*.h)                  │     (forward-looking)
  └──────────┬───────────────────────────────────────────┘
             │
             ▼
  ┌──────────────────────────────────────────────────────┐
  │  OS / GPU Backends                                    │
  │  Win32 GDI DIB framebuffer (live)                    │
  │  Vulkan / D3D12 / WebGPU (catalog-only, Phase 2)     │
  │  stb_truetype single-header font rasterization       │
  └──────────────────────────────────────────────────────┘

  ┌──────────────────────────────────────────────────────┐
  │  widgets/ (DEPRECATED — Phase 5 deletion target)      │  ← 1,559-line C widget
  │  ui_widget.c / ui_widget.h / test_widgets.c / stubs  │     library retained for
  │                                                        │     existing demos; new
  │                                                        │     work uses the Kain
  │                                                        │     component layer.
  └──────────────────────────────────────────────────────┘
```

### 4-Layer Stack (KUIF)

The runtime now serves the KUIF 4-layer architecture:

| Layer | Owner | What |
|-------|-------|------|
| **L3 — Components** | Kain (forthcoming in Phase 4) | `component Button ...` — props, state, JSX, `pulse`/`resonate` |
| **L2 — Semantic Graph** | Kain (compiler) | `world`+`surface`, `entangle`, `patch`, `law`, `resonate`, `pulse` |
| **L1 — Component Surface** | C ABI contract | `KainComponentSurface` vtable — 24 slots, GPU backend routing |
| **L0 — C Substrate** | C runtime | draw primitives, damage tracking, input events, font rasterization |

The Phase 1 implementation delivers **L0 + L1** in C. L2 is the existing
Kain semantic graph. L3 is the next phase.

---

## The C Substrate (src/ui/kain/)

7 widget-free modules with twin headers in `include/kain_*.h`. Internal UI
code includes the local copies; stdlib bridges and external consumers
include from `include/`.

### 1. `kain_geometry.h` (~215 lines)
**Primitive geometry types — pure math, no dependencies.**

- `kainRect { x, y, w, h }`, `kainPoint { x, y }`, `kainSize { w, h }`
- `kainColor { r, g, b, a }` — float [0..1] for GPU compatibility
- `kainMatrix` — 2D affine (translate/scale/rotate), row-major m[6]
- 23 inline helpers: `kain_rect_make`, `kain_rect_contains`, `kain_rect_overlaps`,
  `kain_rect_intersect`, `kain_rect_union`, `kain_point_add/sub`, `kain_color_from_u32`,
  `kain_color_to_u32`, `kain_color_lerp`, `kain_color_clamp`, `kain_matrix_identity`,
  `kain_matrix_translate`, `kain_matrix_scale`, `kain_matrix_rotate`, `kain_matrix_mul`,
  `kain_matrix_transform_point`, `kain_clampf`.
- Predefined color constants: `KAIN_COLOR_TRANSPARENT`, `KAIN_COLOR_BLACK`,
  `KAIN_COLOR_WHITE`, `KAIN_COLOR_RED/GREEN/BLUE`, `KAIN_COLOR_DARK_BG` (#1A1A24).

### 2. `kain_render_software.{h,c}` (~580 + 110 lines)
**16 backend-agnostic draw primitives. No tree-walking, no widgets, no layout.**

```c
// Lifecycle
KainSoftwareRenderer* kain_renderer_create(int w, int h, uint32_t* fb);
void                  kain_renderer_destroy(KainSoftwareRenderer* r);
void                  kain_renderer_set_framebuffer(KainSoftwareRenderer* r, uint32_t* fb, int w, int h);
void                  kain_renderer_set_font_session(KainSoftwareRenderer* r, int64_t session_id);

// Frame
void kain_renderer_clear(KainSoftwareRenderer* r, kainColor color);
void kain_renderer_submit(KainSoftwareRenderer* r);
void kain_renderer_present(KainSoftwareRenderer* r);

// 16 draw primitives
void kain_render_fill_rect          (KainSoftwareRenderer*, kainRect, kainColor);
void kain_render_fill_rounded_rect  (KainSoftwareRenderer*, kainRect, float radius, kainColor);
void kain_render_stroke_rect        (KainSoftwareRenderer*, kainRect, float thickness, kainColor);
void kain_render_fill_circle        (KainSoftwareRenderer*, kainPoint center, float radius, kainColor);
void kain_render_stroke_circle      (KainSoftwareRenderer*, kainPoint center, float radius, float thickness, kainColor);
void kain_render_blit               (KainSoftwareRenderer*, kainRect src, kainRect dst, int64_t texture_id);
void kain_render_text               (KainSoftwareRenderer*, kainPoint pos, const char* text, int64_t font_id, float size, kainColor);
void kain_render_gradient_rect      (KainSoftwareRenderer*, kainRect, const kainColor* colors, const float* stops, int count);
void kain_render_blur               (KainSoftwareRenderer*, kainRect, float radius);

// Clip stack (max 16)
void kain_render_push_clip(KainSoftwareRenderer*, kainRect);
void kain_render_pop_clip (KainSoftwareRenderer*);

// Transform stack (max 16)
void kain_render_push_transform(KainSoftwareRenderer*, kainMatrix);
void kain_render_pop_transform (KainSoftwareRenderer*);
```

Strict aliasing: all dual-pixel writes use `memcpy()`, not `uint64_t*` casts.
Z3-proven: branchless clamp, dual-pixel fill equivalence, corner tests.

### 3. `kain_compositor.{h,c}` (~140 lines)
**Damage region tracker — dirty-rect accumulator with 64-rect ceiling per frame.**

```c
KainCompositor* kain_compositor_create(int fb_width, int fb_height);
void            kain_compositor_destroy(KainCompositor* c);
void            kain_compositor_begin_frame(KainCompositor* c);  // reset accumulator
void            kain_compositor_end_frame(KainCompositor* c);    // compute union_rect
void            kain_compositor_damage_rect(KainCompositor* c, float x, float y, float w, float h);
void            kain_compositor_damage_node(KainCompositor* c, int64_t node_id);  // stub in Phase 1
kainRect        kain_compositor_damaged_region(KainCompositor* c);
bool            kain_compositor_has_damage(KainCompositor* c);
void            kain_compositor_clear_damage(KainCompositor* c);
```

`KainNativeUiSession` carries a `KainCompositor*` field. Per-frame lifecycle
is the unit of damage tracking. Bounding union is computed at `end_frame`.

### 4. `kain_input.{h,c}` (~125 lines)
**Typed event pipeline — thin wrapper over the existing ui_system event queue.**

```c
typedef enum KainInputEventKind {
    KAIN_INPUT_NONE = 0, KAIN_INPUT_KEY_DOWN, KAIN_INPUT_KEY_UP, KAIN_INPUT_TEXT,
    KAIN_INPUT_POINTER_DOWN, KAIN_INPUT_POINTER_UP, KAIN_INPUT_POINTER_MOVE,
    KAIN_INPUT_POINTER_WHEEL, KAIN_INPUT_FOCUS_IN, KAIN_INPUT_FOCUS_OUT,
    KAIN_INPUT_DRAG, KAIN_INPUT_DROP,
} KainInputEventKind;

typedef struct KainInputEvent {
    KainInputEventKind kind;
    int64_t  key_code;       // platform key code or 0
    float    x, y;           // pointer position (client space)
    float    delta_x, delta_y; // scroll/drag delta
    char     text[16];       // UTF-8 text for text input events
    int64_t  device_id, timestamp_ms;
} KainInputEvent;

KainInputPipeline* kain_input_pipeline_create(int64_t session_id);
void               kain_input_pipeline_destroy(KainInputPipeline*);
bool               kain_input_poll_event(KainInputPipeline*, KainInputEvent* out);
void               kain_input_push_event(KainInputPipeline*, const KainInputEvent*);
int64_t            kain_input_hit_test(KainInputPipeline*, float x, float y);
const char*        kain_input_event_type_name(KainInputEventKind);
```

Maps string-based event kinds to a typed enum. Delegates hit-testing to
`abi_ui_hit_test`. Does NOT create new event infrastructure — wraps the
existing ui_system ABI.

### 5. `kain_font.{h,c}` (~160 lines)
**Font load/measure/glyph — extracted from `ui_widget.c` and `native_ui_surface.c`.**

```c
typedef struct KainFontMetrics {
    int   ascent, descent, line_gap;
    float scale;
} KainFontMetrics;

int64_t  kain_font_load         (int64_t session_id, const uint8_t* ttf, int64_t ttf_len, float size);
int64_t  kain_font_load_path    (int64_t session_id, const char* filepath, float size);
int64_t  kain_font_load_default (int64_t session_id, float size);  // KAIN_UI_FONT env override → platform defaults
void*    kain_font_get_glyph    (int64_t session_id, int64_t font_id, int codepoint);
void     kain_font_release_glyph(void* glyph);
float    kain_font_measure_text (int64_t session_id, int64_t font_id, const char* text);
float    kain_font_line_height  (int64_t session_id, int64_t font_id);
KainFontMetrics kain_font_get_metrics(int64_t session_id, int64_t font_id);
```

Platform font path search priority:
- Windows: `C:/Windows/Fonts/segoeui.ttf` → `arial.ttf` → `tahoma.ttf` → `consola.ttf`
- macOS: `/System/Library/Fonts/Helvetica.ttc` → `SFNS.ttf` → `/Library/Fonts/Arial.ttf`
- Linux: `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf` → `TTF/DejaVuSans.ttf`

All loaders gracefully return 0 on missing files / corrupt TTF — no crash.

### 6. `kain_host.h` (~120 lines) + `kain_host_win32.c` (~670 lines)
**Platform-agnostic host vtable. Win32 GDI implementation in Phase 1.**

```c
typedef enum kainHostPlatform {
    KAIN_HOST_UNKNOWN = 0, KAIN_HOST_WIN32, KAIN_HOST_X11, KAIN_HOST_WAYLAND,
    KAIN_HOST_MACOS, KAIN_HOST_WASM,
} kainHostPlatform;

typedef struct kainHostVTable {
    // Identification
    const char*        (*backend_id)(void);
    kainHostPlatform   (*platform)(void);
    // Window lifecycle
    void*              (*window_create)(const char* title, int width, int height);
    void               (*window_destroy)(void* state);
    void               (*window_set_title)(void* state, const char* title);
    void               (*window_set_size)(void* state, int width, int height);
    void               (*window_get_size)(void* state, int* out_w, int* out_h);
    float              (*window_get_dpi)(void* state);
    // Message pump
    void               (*pump_events)(void* state);
    int                (*should_close)(void* state);
    // Framebuffer
    uint32_t*          (*get_framebuffer)(void* state, int* out_stride_elems);
    int                (*get_framebuffer_width)(void* state);
    int                (*get_framebuffer_height)(void* state);
    // Present
    void               (*present)(void* state, void* session);
    // Clipboard
    int                (*clipboard_set_text)(void* state, const char* text);
    int                (*clipboard_get_text)(void* state, char* out, size_t cap);
    // Cursor
    void               (*set_cursor)(void* state, kainHostCursor cursor);
    // GPU surface
    void*              (*get_gpu_surface)(void* state);
} kainHostVTable;

const kainHostVTable* kain_host_get(kainHostPlatform);
const kainHostVTable* kain_host_native(void);
kainHostPlatform      kain_host_current_platform(void);
const char*           kain_host_platform_name(kainHostPlatform);
```

`kain_host_win32.c` implements the vtable for Win32: `KainWin32UI` window
class, `CreateDIBSection` top-down 32-bit DIB framebuffer, `PeekMessage`/
`TranslateMessage`/`DispatchMessage` event pump, BitBlt present via WM_PAINT,
`SetProcessDpiAwarenessContext` per-monitor DPI v2, OS input events bridged
to the universal input system. `ui_host_adapter.c` (272 lines) is now a
thin dispatcher that routes `abi_ui_host_*` calls through `kain_host_get()`.

### 7. `kain_surface.h` (~60 lines)
**GPU surface abstraction (forward-looking — Phase 1 stub).**

```c
typedef enum kainSurfaceKind {
    KAIN_SURFACE_SOFTWARE = 0, KAIN_SURFACE_VULKAN,
    KAIN_SURFACE_D3D12,        KAIN_SURFACE_WEBGPU,
} kainSurfaceKind;

typedef struct kainSurface kainSurface;  // opaque

kainSurface*     kain_surface_create(int width, int height, kainSurfaceKind kind);
void             kain_surface_destroy(kainSurface* s);
void             kain_surface_resize(kainSurface* s, int width, int height);
uint32_t*        kain_surface_pixels(kainSurface* s, int* out_w, int* out_h, int* out_stride);
kainSurfaceKind  kain_surface_backend(kainSurface* s);
int              kain_surface_width(kainSurface* s);
int              kain_surface_height(kainSurface* s);
const char*      kain_surface_kind_name(kainSurfaceKind);
```

In Phase 1 only the SOFTWARE path is active. Vulkan/D3D12/WebGPU will be
filled by Phase 2 (`kain_render_vulkan.c`, `kain_render_webgpu.c`).

---

## The KainComponentSurface Vtable — 24 Slots

The ABI contract between the Kain compiler and any rendering backend.
**sizeof = 24 * sizeof(void*) = 192 bytes on x64.** Slot order is absolute;
NEVER insert, reorder, or delete — only append. The full declaration lives
in `runtime/native/include/component_surface.h`.

| # | Slot | Purpose |
|---|------|---------|
| 0  | `session_create`            | Create UI session + (Win32) attach `winit` host |
| 1  | `session_destroy`           | Tear down session + host |
| 2  | `element_begin`             | Find-or-create node by stable_key (reconciliation) |
| 3  | `element_end`               | No-op for retained-mode |
| 4  | `element_set_text`          | Set text on node |
| 5  | `element_set_attr_i64`      | i64 style attribute |
| 6  | `element_set_attr_f64`      | f64 style attribute |
| 7  | `element_set_attr_string`   | String style attribute (fill_color, border_color, ink_color, title) |
| 8  | `state_get_i64`             | i64 component state (with fallback) |
| 9  | `state_set_i64`             | i64 component state write |
| 10 | `begin_frame`               | Begin frame (delta_ms) |
| 11 | `end_frame`                 | End frame |
| 12 | `present`                   | Blit framebuffer → InvalidateRect → BitBlt |
| 13 | `poll_event`                | Poll next event (with out_event/max_size) |
| 14 | `should_close`              | Window close signal |
| 15 | `window_open`               | Open named window |
| 16 | `host_pump`                 | Pump OS messages |
| 17 | `session_attach_platform`   | Receive native window handle (for GPU WSI) |
| 18 | `get_gpu_extension`         | Return `KainGpuSurfaceExtension*` (NULL for software) |
| 19 | `state_get_f64`             | Float state (slider value, opacity, animation progress) |
| 20 | `state_set_f64`             | Float state write |
| 21 | `state_get_string`          | String state (textbox content) |
| 22 | `state_set_string`          | String state write |
| 23 | `element_set_callback`      | Bind event callback fn pointer on element |

**State persistence** (slots 8-9, 19-22) is implemented on a hidden
`__kain_state_root` node — created lazily, marked with the `hidden` flag
so it never appears in hit-tests, draw walks, or bundle serialization.

**Callback binding** (slot 23) stores `void(*)(void)` function pointers
on the target element node. When an event fires, the UI system invokes
the callback via the internal helper `abi_ui_node_invoke_callback()`
(not a vtable slot — it lives in `ui_system.c`).

### GPU Backend Routing

The same 24-slot vtable is implemented by every backend. The compiler
resolves the surface once per world via `kain_component_surface_resolve()`,
then calls through the vtable every frame:

| Backend | Status | Surface |
|---------|--------|---------|
| `native_ui` (software GDI) | **Live** — `native_ui_surface.c` | `winit` HWND, DIB framebuffer, BitBlt |
| `vulkan` | Catalog-only (Phase 2 target) | `extras/vulkan-abi/vulkan_abi.c` |
| `d3d12` | Catalog-only (Phase 2 target) | `extras/d3d12-abi/d3d12_abi.c` |
| `webgpu` | Catalog-only (Phase 2 target) | `extras/webgpu-abi/webgpu_abi.c` |

`native_ui` auto-registers at startup via CRT constructor (MSVC `.CRT$XCU`
or GCC/Clang `__attribute__((constructor))`). GPU backends register
through the same `kain_component_surface_register()` API.

### ABI Stability

`abi_ui_*` exports in `include/ui_system.h` (~174 functions) are the
**frozen** contract that LLVM-emitted code calls via `@extern` in
`stdlib/ui.kn`. Phase 1 was an internal refactor — every `kain_*`
addition is non-breaking. New ABI exports for slots 19-22 were added
(`abi_ui_node_state_f64`, `abi_ui_node_set_state_f64`, etc.) and
slot 23 (`abi_ui_node_set_callback`).

---

## Source File Inventory

### C Runtime Substrate (src/ui/) — 12 source files, ~9,870 lines

| File | Lines | Role |
|------|-------|------|
| `ui_system_internal.h` | 252 | Internal types: `KainNativeUiNode`, `KainNativeUiSession`, `KainCompositor* compositor` field |
| `ui_system.c` | 3,034 | Core engine: session lifecycle, node CRUD, style/state, event ring, focus, IME, drag-drop, menus, dialogs, resources, fonts, callback dispatch (slot 23), per-frame arena |
| `ui_host_adapter.c` | 272 | Thin dispatcher — routes `abi_ui_host_*` through `kain_host_get()` (Win32 GDI in Phase 1) |
| `ui_host_adapter.h` | 34 | Minimal host adapter interface |
| `ui_renderer.c` | 244 | Tree-walking renderer — UNCHANGED signature, **internally refactored** to call `kain_render_*` primitives from `kain_render_software.c` |
| `ui_layout.c` | 199 | Flexbox-style layout engine: direction, padding, spacing, gap, recursive resolution |
| `ui_color.c` | 237 | Color parsing (#hex, rgba, named), Z3-proven `div255_fast` alpha blending, opacity |
| `ui_hot_reload.c` | 710 | Shared-memory IPC for live UI reload, file signature watcher, controller |
| `ui_runtime.c` | 1,283 | High-level compiled-bundle runtime: validation, focus routing, event routing, text editing, hot-reload state transfer |
| `ui_compiled_bundle.c` | 783 | JSON bundle deserializer for compiler-compiled UI trees |
| `native_ui_surface.c` | 438 | `KainComponentSurface` vtable impl — reference software/GDI backend, auto-registers as "native_ui" |
| **(deprecated)** `widgets/ui_widget.c` | 1,559 | C widget library — kept for existing demos, Phase 5 deletion target |
| **(deprecated)** `widgets/ui_widget.h` | 273 | C widget header with 19 `#define UI_COLOR_*` and 11 `#define UI_*_SIZE` |

### C Substrate (src/ui/kain/) — 7 modules, ~2,150 lines

| File | Lines | Role |
|------|-------|------|
| `kain_geometry.h` | 216 | Primitive types + 23 inline math helpers |
| `kain_render_software.h` | 112 | 16 draw primitive signatures + clip/transform stack |
| `kain_render_software.c` | 582 | 16 primitives, branchless clamp, dual-pixel fill via memcpy |
| `kain_compositor.h` | 54 | Damage region tracker header |
| `kain_compositor.c` | 139 | Dirty-rect accumulator, 64-rect ceiling, frame-bounded |
| `kain_input.h` | 72 | 11 event kinds, `KainInputEvent` struct, typed pipeline |
| `kain_input.c` | 126 | Thin wrapper over `abi_ui_push_event`/`abi_ui_poll_event` |
| `kain_font.h` | 76 | Font load/measure/glyph interface |
| `kain_font.c` | 161 | Font path search extracted from `ui_widget.c`; wraps `abi_ui_font_load_ttf` |
| `kain_host.h` | 118 | Platform-agnostic host vtable (15 slots) + platform dispatch |
| `kain_host_win32.c` | 669 | Win32 GDI backend: DIB framebuffer, window class, message pump, BitBlt present |
| `kain_surface.h` | 61 | GPU surface abstraction (Phase 1 stub; Vulkan/D3D12/WebGPU in Phase 2) |

### Test Suites (src/ui/test_ui*/) — 23 test/demo files

| Directory | Files | Lines | Status |
|-----------|-------|-------|--------|
| `test_ui/`     | 10 C tests + `stubs.c` + `calculator.kn` | 4,997 | Regression tests (v1) |
| `test_ui_v2/`  | 6 demos + `stubs.c` + `build.bat`     | 8,427 | Visually-rich demos (v2) |
| `test_ui_v3/`  | 7 demos + `build.bat`                  | 3,797 | **NEW** — exercise Phase 1 substrate directly |
| `debug_ui/`    | 18 diagnostic tests                    | ~3,200 | Diagnostic + minimal-path tests |
| `fuzz/`        | 7 fuzzers + fuzzer.{c,h} + stubs + run_fuzz.py + fuzz_taxonomy.json | ~3,500 | **NEW** — data-driven fuzz suite |

### Fuzz Suite (fuzz/)

| File | Purpose |
|------|---------|
| `fuzzer.c` / `fuzzer.h` | Main entry: config parsing, domain dispatch, telemetry, report writer |
| `geometry_fuzzer.c` | Fuzz `kain_geometry.h` — rect ops, color math, matrix/point transforms |
| `render_fuzzer.c` | Fuzz 16 `kain_render_*` primitives + clip/transform stacks |
| `compositor_fuzzer.c` | Fuzz damage rect tracking, overflow, frame sequences |
| `input_fuzzer.c` | Fuzz event push/poll/hit-test with floods and extremes |
| `font_fuzzer.c` | Fuzz corrupt TTF loading, glyph access, text measurement |
| `surface_fuzzer.c` | Fuzz create/destroy/resize with edge params + all backend kinds |
| `vtable_fuzzer.c` | Fuzz all 24 `KainComponentSurface` slots via `native_ui` |
| `fuzz_taxonomy.json` | **Data-driven taxonomy** — valid ranges, boundary values, crash reproduction |
| `run_fuzz.py` | Python orchestrator: builds C fuzzer, runs iterations, generates Markdown reports |
| `reports/` | Generated timestamped Markdown reports with full telemetry |

### Z3 Proof Packs (z3/)

| Subdir | Count | Notes |
|--------|:-----:|-------|
| `z3/proofs/c/` | 35 YAML | Renderer + runtime correctness (color blend, layout, sibling bounds, etc.) |
| `z3/proofs-experimental/` | 62 SMT2 | Branchless clamp, alpha blend, flag batch, SIMD fill, dirty flag, hash, index bounds, event ring, draw command, layout, etc. |
| `extras/_stb-truetype/z3/proofs/` | 21 SMT2 + 21 YAML | Font rasterization correctness |

---

## Public Headers (include/)

### Substrate Headers — 7 twin files in `include/`

| Header | Lines | Purpose |
|--------|-------|---------|
| `kain_geometry.h` | 216 | `kainRect`, `kainPoint`, `kainSize`, `kainColor`, `kainMatrix` + helpers |
| `kain_render_software.h` | 112 | 16 draw primitive signatures + clip/transform stack |
| `kain_compositor.h` | 54 | Damage region tracker |
| `kain_input.h` | 72 | Typed event pipeline |
| `kain_font.h` | 76 | Font load/measure/glyph |
| `kain_host.h` | 118 | Platform-agnostic host vtable |
| `kain_surface.h` | 61 | GPU surface abstraction |

These are **twin copies** of the headers in `src/ui/kain/`. Internal UI
code includes the local copy from `src/ui/kain/`; stdlib bridges and
external consumers include from `include/`.

### ABI Headers (frozen surface for compiled Kain)

| Header | Exports | Purpose |
|--------|--------:|---------|
| `component_surface.h` | 1 struct + 3 fns | `KainComponentSurface` vtable + registry |
| `ui_system.h` | 174 fns | All `abi_ui_*` exports — frozen ABI for LLVM-emitted `@extern` |
| `ui_runtime.h` | ~30 fns | High-level compiled-bundle runtime |
| `ui_color.h` | ~10 fns | Color parse/blend helpers |
| `ui_font.h` | ~12 fns | Font + glyph + measure |
| `ui_layout.h` | ~5 fns | Layout engine types |
| `ui_renderer.h` | ~5 fns | Renderer capability descriptors |
| `ui_bundle.h` | ~15 fns | Compiled UI bundle loading |
| `ui_hot_reload.h` | ~10 fns | Hot reload descriptors |
| `gpu_surface_extension.h` | 1 struct + 3 fns | `KainGpuSurfaceExtension` (slot 18) |

---

## Public API (include/ui_system.h)

~174 `abi_ui_*` functions in 14 categories:

| Category | Examples |
|----------|----------|
| **Session** | `abi_ui_session_create`, `abi_ui_session_destroy`, `abi_ui_session_count`, `abi_ui_reset` |
| **Frame** | `abi_ui_begin_frame`, `abi_ui_end_frame`, `abi_ui_present`, `abi_ui_host_attach`, `abi_ui_host_pump`, `abi_ui_host_present`, `abi_ui_host_should_close`, `abi_ui_host_backend` |
| **Node** | `abi_ui_node_create`, `abi_ui_node_destroy`, `abi_ui_node_set_parent`, `abi_ui_node_set_rect`, `abi_ui_node_set_text`, `abi_ui_node_set_stable_key`, `abi_ui_node_find_by_stable_key`, `abi_ui_node_set_flag`, `abi_ui_node_set_callback` |
| **Styles** | `abi_ui_node_set_style_i64/f64/string`, `abi_ui_node_get_style_*` |
| **State** | `abi_ui_node_state_i64`, `abi_ui_node_set_state_i64`, `abi_ui_node_state_f64`, `abi_ui_node_set_state_f64`, `abi_ui_node_state_string`, `abi_ui_node_set_state_string` |
| **Events** | `abi_ui_push_event`, `abi_ui_poll_event`, `abi_ui_event_kind/target/x/y/key_code/text`, `abi_ui_hit_test` |
| **Resources** | `abi_ui_resource_create`, `abi_ui_resource_set_bytes`, `abi_ui_resource_set_bytes_hex` |
| **Fonts** | `abi_ui_font_create`, `abi_ui_font_load_ttf`, `abi_ui_font_get_glyph`, `abi_ui_font_release_glyph`, `abi_ui_font_get_vmetrics`, `abi_ui_text_measure_width/height` |
| **Focus** | `abi_ui_focus`, `abi_ui_focused_node` |
| **IME** | `abi_ui_ime_begin`, `abi_ui_ime_commit_text`, `abi_ui_ime_end` |
| **Drag** | `abi_ui_drag_begin`, `abi_ui_drag_update`, `abi_ui_drag_drop` |
| **Menus** | `abi_ui_menu_create`, `abi_ui_menu_add_item`, `abi_ui_menu_open` |
| **Dialogs** | `abi_ui_dialog_request`, `abi_ui_dialog_respond`, `abi_ui_dialog_poll_response` |
| **Hot Reload** | `abi_ui_hot_reload_begin`, `abi_ui_hot_reload_commit` |

---

## Build System

### Master Makefile (`src/ui/Makefile`)

```bash
# Build everything
make                     # static lib + tests + demos
make -j8                 # parallel build (8 jobs)

# Individual targets
make static              # build libkain_ui.a
make libs                # build .o files only (incremental)
make tests               # build test_ui/ executables
make demos               # build test_ui_v2/ demo executables
make widgets             # build widget library test

# Build + run
make run_cosmic          # cosmic dashboard
make run_retro           # retro wave
make run_3d              # 3D sandbox
make run_flp             # fire + life + plasma
make run_voxel           # isometric voxel viewer
make run_calc            # calculator test

# Fuzz suite
make fuzz                # build fuzzer binary
make fuzz-quick          # 10k iterations
make fuzz-run            # 50k iterations
make fuzz-stress         # 500k iterations
make fuzz-clean          # remove fuzz build artifacts
make fuzz-report         # regenerate report from saved JSON

# Options
make CC=gcc              # use GCC instead of clang
make CFLAGS=-O2          # optimized build

# Clean
make clean
```

**Key features:**
- Incremental compilation — `.o` files cached, only changed `.c` files recompile
- Auto-generated `.d` files track header dependencies
- Static library aggregates all UI + core + substrate objects
- MSVC auto-detection — finds Visual Studio + SDK library paths
- VPATH: sources across `src/ui/`, `src/ui/kain/`, `src/ui/widgets/`, `src/core/`

### Source Organization in Makefile

| Group | Source files | Purpose |
|-------|--------------|---------|
| `UI_SRCS`   | 9 files | Core UI system (ui_system, ui_renderer, ui_layout, etc.) |
| `CORE_SRCS` | 2 files | component_surface.c, input_system.c (cross-referenced) |
| `KAIN_SRCS` | 5 files | Phase 1 substrate (kain_render_software, kain_compositor, etc.) |
| `WIDGET_SRCS` | 1 file | widgets/ui_widget.c (deprecated, Phase 5 deletion) |

### Per-Suite Build Scripts

| Script | Suite |
|--------|-------|
| `test_ui_v3/build.bat` | New v3 demos (7 targets) |
| `test_ui_v2/build.bat` | v2 demos (6 targets) |
| `test_ui/build.bat` | v1 tests |
| `widgets/build.bat` | Widget library |

### TOML Manifest

The UI runtime sources are listed in `runtime/native_core_runtime.toml`:
- `src/ui/ui_system.c`, `ui_host_adapter.c`, `ui_renderer.c`, `ui_layout.c`, `ui_color.c`, `ui_hot_reload.c`, `ui_runtime.c`, `ui_compiled_bundle.c`, `native_ui_surface.c`
- `src/ui/kain/kain_render_software.c`, `kain_compositor.c`, `kain_input.c`, `kain_font.c`, `kain_host_win32.c`
- `src/ui/widgets/ui_widget.c` (deprecated)
- `src/core/component_surface.c`, `src/core/input_system.c`

If you change a C file in this directory, run:
```powershell
py -3 scripts/python/update_runtime.py
```
to regenerate the Bazel BUILD files.

---

## Tests

### `test_ui/` — 10 regression tests (4,997 lines)

| Test | Lines | What It Demonstrates |
|------|:-----:|----------------------|
| `calculator.c` | 506 | Working 4-function calculator with click/keyboard input, styled buttons, real arithmetic |
| `full_demo.c` | 505 | Rich dashboard with sidebar, animated cards, live bar chart, input logging, status bar |
| `keypad.c` | 508 | PIN entry with masked display, visual feedback, access granted/denied state machine |
| `anim_demo.c` | 441 | 100-particle physics simulation with bouncing, color cycling, opacity |
| `hot_reload_test.c` | 298 | Shared memory IPC channel test |
| `font_test.c` | 366 | Font loading + glyph rasterization |
| `renderer_smoke_test.c` | 389 | First test to call `ui_render_frame()` with real nodes — Oracle-verified |
| `widget_hello.c` | 478 | Minimal widget library hello world |
| `widget_demo.c` | 818 | Comprehensive widget demo |
| `widget_calculator.c` | 654 | Calculator built with widget system |

### `test_ui_v2/` — 6 visually-rich demos (8,427 lines)

| Demo | Lines | What It Demonstrates |
|------|:-----:|----------------------|
| `cosmic_dashboard.c` | 1,790 | 350 parallax particles, nebula gradients, 6 glass-morphism panels, 10 fonts, waveform, rotating particle flux ring, command console, constellation chart, orbiting compass |
| `retrowave.c` | 1,580 | Scrolling perspective grid, neon sunset with scanlines, 3-axis wireframe cube, 5 transparent glowing panels, 8-bar equalizer, Matrix rain, bouncing cassettes, 3 color schemes, multi-pass glow, glitch effect |
| `ui3d_sandbox.c` | 1,340 | 3D rotation matrices, perspective/orthographic projection, painter's algorithm, isometric grid floor, Z-depth floating panels, particle fountain with gravity, 120-star parallax starfield, exploding cube animation |
| `voxel_viewer.c` | 1,386 | Isometric voxel viewer |
| `fire_life_plasma.c` | 1,121 | Fire + Life + Plasma simulation overlays |
| `font_inferno.c` | 1,185 | Multi-font stress test |

### `test_ui_v3/` — 7 NEW substrate demos (3,797 lines)

These demos exercise the **refactored Phase 1 C substrate directly** — no
`ui_widget.c` dependency, no hardcoded color `#define`s, no tree-walking
renderer. They drive `kain_render_*`, `kain_compositor_*`, `kain_input_*`,
`kain_font_*`, `kain_host_*`, and the 24-slot vtable directly.

| Demo | Lines | What It Exercises |
|------|:-----:|-------------------|
| `v3_render_primitives.c` | 540 | All 16 `kain_render_*` draw primitives + clip stack + transform stack |
| `v3_compositor_damage.c` | 444 | `kain_compositor_*` — damage_rect accumulation, damaged_region union, frame lifecycle, partial redraw with visual damage regions |
| `v3_input_pipeline.c` | 584 | `kain_input_*` — keyboard, mouse, scroll, hit-test, event kind names |
| `v3_font_substrate.c` | 513 | `kain_font_*` — load (path + default), metrics, line height, multi-font text at various sizes |
| `v3_host_vtable.c` | 466 | `kainHostVTable` — window create/destroy, resize, DPI query, clipboard, cursor, framebuffer access via `kain_host_get()` |
| `v3_surface_vtable.c` | 564 | `KainComponentSurface` 24-slot vtable — session lifecycle, element tree, frame lifecycle, f64/String state, callbacks |
| `v3_combined_demo.c` | 686 | All of the above fused into one interactive dashboard |

### `fuzz/` — Data-Driven Fuzz Suite

7 fuzzers, data-driven by `fuzz_taxonomy.json`, 10,000-500,000 iterations
per run, Markdown reports in `fuzz/reports/`. See `fuzz/README.md` for
the full reference. Verification gate: `python fuzz/run_fuzz.py --stress`.

### `debug_ui/` — Diagnostic + minimal-path tests

18 C files exercising specific code paths and crash conditions. Includes
`path_a_full_pipeline.c`, `path_b_direct_fb.c`, `path_c_pure_gdi.c`,
`minimal_win32_test.c`, `fb_input_test.c`, `diagnostic_minimal.c`, etc.

---

## Demos Summary

| Suite | Files | Lines | Era |
|-------|:-----:|:-----:|-----|
| `test_ui/` | 10 | 4,997 | v1 regression |
| `test_ui_v2/` | 6 | 8,427 | v2 visually-rich |
| `test_ui_v3/` | 7 | 3,797 | v3 substrate (NEW) |
| `debug_ui/` | 18 | ~3,200 | diagnostics |
| **Total** | **41** | **~20,400** | |

All demos build and run on Win32. Build via `make` (master Makefile) or
per-suite `build.bat`.

---

## Z3 Proof Coverage

### Renderer + Runtime Correctness (z3/proofs/c/) — 35 YAML proofs

| Proof | What It Proves |
|-------|---------------|
| `ui-renderer-sibling-bounds-safe.yaml` | `child_idx` always in valid `[0, MAX_NODES-1]` range |
| `ui-renderer-children-always-traversed.yaml` | Children rendered regardless of parent size |
| `ui-renderer-fb-clear-no-aliasing.yaml` | `memcpy` framebuffer clear is strict-aliasing safe |
| `ui-layout-no-stack-overflow.yaml` | Stack allocation bounded < 2KB |
| `ui-color-rgba-bitfield-no-overlap.yaml` | Color channel bitfields don't overlap |
| `ui-color-hex-nibble-expand.yaml` | 4-bit hex expansion is correct |
| `ui-runtime-find-node-index-bounds.yaml` | Runtime node index lookups bounded |
| `ui-runtime-focus-search-termination.yaml` | Focus search always terminates |
| `ui-runtime-text-append-bounds.yaml` | Text append buffer never overflows |
| `ui-runtime-validation-report-bounds.yaml` | Validation report bounded |
| `ui_append_draw_command_count_bounded.yaml` | Draw command ring never overflows |
| `ui_push_event_event_count_bounded.yaml` | Event ring never overflows |
| `ui_poll_event_event_count_no_underflow.yaml` | Event ring underflow guarded |
| `ui_event_head_tail_mask_bounds.yaml` | Event head/tail always in valid range |
| `ui_host_adapter_lifecycle_state_machine.yaml` | Host adapter state machine is total |
| `ui_host_adapter_shutdown_nulls_host_state.yaml` | Shutdown nulls host_state safely |
| `ui_index_start_slot_u64_mask_bounds.yaml` | Open-addressing start slot in bounds |
| `ui_low_bit_index_u64-debruijn-signature-unique.yaml` | De Bruijn sequence unique |
| `ui_isolate_low_bit_u64_power_of_two.yaml` | Low-bit isolation correct for power-of-two |
| `ui_node_create-generic-size-add.yaml` | Node create size check correct |
| `ui_node_destroy-generic-size-add.yaml` | Node destroy size check correct |
| `ui_find_state-generic-size-add.yaml` | State lookup size check correct |
| `ui_ensure_state-generic-size-add.yaml` | State ensure size check correct |
| `ui_find_style-generic-size-add.yaml` | Style lookup size check correct |
| `ui_ensure_style-generic-size-add.yaml` | Style ensure size check correct |
| `ui_hot_reload_ring_index_bounds.yaml` | Hot reload ring index bounded |
| `ui_host_present-generic-size-add.yaml` | Host present size check correct |
| `ui_win32_gl_process_menu-generic-size-add.yaml` | Win32 menu processing size check correct |
| `ui_win32_gl_render-generic-size-add.yaml` | Win32 render size check correct |
| `ui_decode_hex-generic-size-add.yaml` | Hex decode size check correct |
| `ui_compiled_bundle_node_count_bounded.yaml` | Bundle node count bounded |
| `ui_layout_zero_division_guard.yaml` | Layout division by zero guarded |
| `native-ui-surface-state-machine.yaml` | Surface state machine correct |
| `native-ui-surface-buffer-size-overflow.yaml` | Surface buffer never overflows |
| `if-generic-size-add.yaml` | Generic size-add branch correct |

### Branchless + Cache Optimizations (z3/proofs-experimental/) — 62 SMT2

Branchless clamp, alpha blend, flag batch test, SIMD fill, sibling-linked
list 4000× speedup, dirty flag 51× speedup, stable key FNV-1a collision,
incremental index update, mask bounds, event count bounded, draw command
count bounded, arena-vs-malloc 25-30× speedup, tagged pointers, hot
reload monotonic counters, native_ui buffer overflow, surface state
machine, perfect hash 4-bit, style lookup bound, renderer pixel bounds.

### Font Proofs (extras/_stb-truetype/z3/proofs/) — 21 SMT2 + 21 YAML

Bezier convex hull, scale/pixel-height division safety, glyph bitmap
bounds non-overflow, hmtx table index bounds, scanline AA coverage
clamping [0, 255], winding rule accumulation, edge clip arithmetic, sort
comparator total order, find table, IsGlyphEmpty, clip line, active
edges, subpixel box, AA coverage, coverage invariant, sort stability.

---

## External Dependencies

| Library | Used For | Header/Link |
|---------|----------|------------|
| **Win32 / user32** | Window creation, message pump, input | `#include <windows.h>`, `-luser32` |
| **GDI / gdi32** | DIB framebuffer, BitBlt, text (legacy tests only) | `-lgdi32` |
| **OpenGL / opengl32** | Legacy, cataloged for future GPU backends | `-lopengl32` |
| **stb_truetype** | Font rasterization (single-header, no link) | `#include "stb_truetype.h"` |
| **Component Surface** | Kain compiler bridge | `component_surface.h`, `component_surface.c` |
| **Input System** | Universal input event bridge | `input_system.h`, `input_system.c` |

### Environment Variables

| Variable | Default | Used By | Purpose |
|----------|---------|---------|---------|
| `ABI_UI_BUNDLE` | — | `ui_compiled_bundle`, `ui_hot_reload` | Path to compiled bundle JSON |
| `ABI_UI_HOT_RELOAD_CHANNEL` | `kain-ui-reload.<app>` | `ui_hot_reload` | Named shared memory channel for IPC reload |
| `ABI_UI_HOT_RELOAD_POLL_INTERVAL_MS` | `125` | `ui_hot_reload` | Poll interval for change detection |
| `KAIN_UI_FONT` | — | `kain_font_load_default`, `native_ui_surface` | Explicit font path override |
| `RENDERER_BACKEND` | — | `component_surface.c` | Selects GPU backend (vulkan/d3d12/webgpu) |
| `LIB` | auto-detected | Makefile | MSVC library search path |

---

## Widget Library (`widgets/`) — DEPRECATED

**Phase 5 deletion target.** Retained for existing demos. New work uses
the Kain component layer.

An **immediate-mode-style** widget library built ON TOP of the retained-mode
ABI. No GDI — all text rendered via stb_truetype glyph rasterization.

| Widget | Function | Returns | State Tracking |
|--------|----------|---------|----------------|
| Button | `ui_button(ctx, label)` | 1 if clicked | normal → hover → pressed → click |
| Label | `ui_label(ctx, text)` | node_id | Static |
| Checkbox | `ui_checkbox(ctx, label, &value)` | 1 if toggled | Toggles `*value` |
| Slider | `ui_slider(ctx, &value, lo, hi)` | 1 if changed | Click-drag thumb |
| Textbox | `ui_textbox(ctx, buf, size)` | 1 if changed | Focus, keyboard input, cursor |
| Panel | `ui_panel(ctx, title, x, y, w, h)` | node_id | Container with title bar |
| Progress | `ui_progress(ctx, value, max)` | — | Visual ratio |
| Window | `ui_window(ctx, title, &x, &y, w, h, &open)` | 1 if open | Draggable, closable |

**Files:** `widgets/ui_widget.h` (273 lines), `widgets/ui_widget.c` (1,559 lines),
`widgets/test_widgets.c` (461 lines), `widgets/stubs.c` (30 lines).

19 `#define UI_COLOR_*` and 11 `#define UI_*_SIZE` constants in `ui_widget.h`
are **two sources of truth** for theme values — Phase 4 replaces them with
`stdlib/ui/theme.kn` Kain structs.

---

## How to Write a New UI Demo

### Minimal skeleton (raw ABI, no widgets):

```c
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ui_system.h"
#include "ui_system_internal.h"
#include "ui_host_adapter.h"
#include "ui_renderer.h"
#include "ui_layout.h"
#include "ui_color.h"

int main(void) {
    int64_t sid = abi_ui_session_create("MyDemo", 1280, 720);
    abi_ui_window_open(sid, "My Demo", 1280, 720);
    abi_ui_host_attach(sid, "winit");

    KainNativeUiSession* s = (KainNativeUiSession*)abi_ui_find_session(sid);
    // ... access host_state, framebuffer via ui_host_adapter internals ...

    while (running) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        abi_ui_begin_frame(sid, dt);
        // Build node tree via abi_ui_node_create / set_parent / set_text / set_style_*
        abi_ui_end_frame(sid);

        // Layout + render
        ui_layout_resolve(s);
        // ... render to framebuffer ...

        InvalidateRect(hwnd, NULL, FALSE);  // triggers WM_PAINT → BitBlt
        Sleep(16);
    }

    abi_ui_session_destroy(sid);
    return 0;
}
```

### With the Phase 1 substrate (direct draw primitives):

```c
#include "kain/kain_render_software.h"
#include "kain/kain_compositor.h"
#include "kain/kain_input.h"
#include "kain/kain_geometry.h"

KainSoftwareRenderer* r = kain_renderer_create(fb_w, fb_h, framebuffer);
KainCompositor* c = kain_compositor_create(fb_w, fb_h);
KainInputPipeline* in = kain_input_pipeline_create(session_id);

while (running) {
    kain_compositor_begin_frame(c);

    // Draw directly to framebuffer
    kain_renderer_clear(r, KAIN_COLOR_DARK_BG);
    kain_render_fill_rounded_rect(r,
        kain_rect_make(100, 100, 400, 200), 16.0f,
        kain_color_rgba(0.13f, 0.13f, 0.25f, 1.0f));
    kain_render_fill_circle(r, kain_point_make(640, 360), 50.0f,
        KAIN_COLOR_RED);
    kain_render_text(r, kain_point_make(120, 150), "Hello, Kain!",
        font_id, 18.0f, KAIN_COLOR_WHITE);

    // Pump events
    KainInputEvent ev;
    while (kain_input_poll_event(in, &ev)) {
        // handle event
    }

    kain_compositor_end_frame(c);
    // present via host (Win32: InvalidateRect + WM_PAINT → BitBlt)
}

kain_input_pipeline_destroy(in);
kain_compositor_destroy(c);
kain_renderer_destroy(r);
```

### With the vtable surface (recommended for new code):

```c
#include "component_surface.h"

const KainComponentSurface* s = kain_component_surface_resolve("native_ui");
if (s) {
    int64_t sid = s->session_create("MyApp", 1280, 720);
    s->begin_frame(sid, 16.0);
    // ... element tree via s->element_begin / set_attr_* / set_text ...
    s->end_frame(sid);
    s->present(sid);
    s->session_destroy(sid);
}
```

---

## Known Limitations

1. **Single-threaded**: All rendering and event processing is single-threaded.
2. **Win32-only host (live)**: The `winit` backend is Windows GDI only. X11,
   Wayland, macOS, and WASM host vtables are forward-declared in `kain_host.h`
   but not implemented.
3. **GPU backends cataloged, not wired**: Vulkan, D3D12, WebGPU surface shims
   exist (`runtime/native/src/core/vulkan_surface_shim.c`, etc.) and the
   `KainComponentSurface` vtable is the ABI for all of them, but the
   `kain_render_vulkan.c` and `kain_render_webgpu.c` renderers that go
   through the vtable are Phase 2 work.
4. **Font hinting**: stb_truetype basic hinting only. No ClearType or
   subpixel rendering.
5. **No widget theming engine**: The deprecated `ui_widget.h` has hardcoded
   color/size constants; new work uses `stdlib/ui/theme.kn` (Phase 4).
6. **Compositor damage_node stub**: `kain_compositor_damage_node()` is a
   Phase 1 stub — it takes a node_id but does not look up the node rect.
   Future phases will integrate with the node tree.
7. **Callback dispatch runtime-only**: `abi_ui_node_invoke_callback()` is
   a C runtime helper. The Kain compiler emits the bind call (slot 23) and
   the runtime invokes the bound fn pointer when events fire. This is not
   yet exercised by end-to-end Kain components (Phase 3 ABI expansion).
8. **Existing C widget layer**: `widgets/ui_widget.c` (1,559 lines) and
   `widgets/ui_widget.h` (273 lines) are deprecated but not yet deleted —
   Phase 5 will remove them after all blades migrate to Kain components.

---

## Related Documentation

| Doc | What |
|-----|------|
| `X:/docs/UI.MD` | Complete UI guide (Phases 1-4) — Kain-side authoring |
| `X:/docs/COMPONENT.MD` | Component reference (1,513 lines) — props, state, JSX |
| `X:/docs/WORLD.MD` | World + surface projection (1,552 lines) |
| `research/MASTER_DOC.md` | KUIF master plan, 4-layer architecture, phase roadmap |
| `research/RENDER-AND-UI-MAP.md` | 122-file dependency map across 16 layers |
| `research/PHASE1_C_SUBSTRATE_FILES.md` | Exact Phase 1 C signatures, build order |
| `research/PHASE3_COMPILER_PIPELINE_FILES.md` | Rust AST/parser/typechecker/codegen changes |
| `research/PHASE4_STDLIB_FILES.md` | Complete `core.kn`, `theme.kn`, all 25 component files |
| `reference/KAIN_VS_CLAY.md` | Brutal Clay comparison — Kain layout vs Clay flexbox |
| `test_ui_v3/README.md` | New v3 substrate demos |
| `fuzz/README.md` | Fuzz suite quick start |

---

*Generated from source analysis after Phase 1-4 KUIF implementation. Total UI subsystem: ~9,870 lines of C across 12 source files in `runtime/native/src/ui/` plus ~2,150 lines of Phase 1 substrate in `runtime/native/src/ui/kain/` plus headers in `runtime/native/include/`.*
