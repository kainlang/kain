# Kain Native UI Runtime

**A retained-mode, cross-platform C11 UI system** — 12 source files, ~7,000 lines of C, built into the Kain runtime substrate. Full pipeline from compiled UI bundles through node tree management, layout resolution, hit-testing, event routing, software/GPU rendering, Win32 window backends, font rasterization, and live hot-reload.

---

## Architecture Overview

```
  APPLICATIONS / KAITANA BLADES / C DEMOS
       │  (KainComponentSurface vtable or raw ABI calls)
       ▼
  ┌──────────────────────────────────────┐
  │  native_ui_surface.c                  │  ← Ecosystem layer: compiler surface adapter
  │  (KainComponentSurface vtable impl)   │     (wraps abi_ui_* for the Kain compiler)
  └──────────┬───────────────────────────┘
             │  abi_ui_* API calls
             ▼
  ┌──────────────────────────────────────┐
  │  ui_system.c        (~2,600 lines)   │  ← Core retained-mode session engine
  │  ui_system_internal.h (~210 lines)    │     Nodes, styles, state, events,
  │                                       │     resources, menus, dialogs, IME,
  │                                       │     drag-drop, clipboard, hot-reload markers
  └──┬────┬────┬────┬────┬────┬────┬────┘
     │    │    │    │    │    │    │
     ▼    ▼    ▼    ▼    ▼    ▼    ▼
  ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌──────┐
  │ui_│ │ui_│ │ui_│ │ui_│ │ui_│ │ui_│ │native│
  │lay │ │ren│ │col│ │com│ │hot│ │run│ │_ui_su│
  │out │ │der│ │or │ │pil│ │rel│ │tim│ │rface │
  │.c  │ │.c │ │.c │ │ed_│ │oad│ │e.c│ │.c    │
  │    │ │   │ │   │ │bun│ │.c │ │   │ │      │
  │    │ │   │ │   │ │dle│ │   │ │   │ │      │
  │    │ │   │ │   │ │.c │ │   │ │   │ │      │
  └──┬─┘ └─┬─┘ └─┬─┘ └───┘ └───┘ └───┘ └──────┘
     │     │      │
     ▼     ▼      ▼
  ┌──────────────────────────────┐
  │  ui_host_adapter.c            │  ← OS window bridge
  │  (Win32 GDI DIB framebuffer)  │     BitBlt, PeekMessage, WM_PAINT
  └──────────┬───────────────────┘
             │
     ┌───────┴───────┐
     ▼               ▼
  Win32 GDI      GPU Backends
  (DIB section)  (Vulkan, D3D12, WebGPU — cataloged)

  ┌──────────────────────────────┐
  │  widgets/ui_widget.*          │  ← Immediate-mode widget library
  │  (button, checkbox, slider,   │     Built ON TOP of abi_ui_*
  │   textbox, panel, label,      │     No GDI text — uses stb_truetype
  │   progress, window)           │
  └──────────────────────────────┘

  ┌──────────────────────────────┐
  │  stb_truetype.h (extras/)     │  ← Font rasterization
  │  14 Z3-proof packs            │     Glyph loading, bitmap rasterization
  └──────────────────────────────┘
```

---

## Source Files (12 files, ~7,000 lines)

| File | Lines | Role | Key Dependencies |
|------|-------|------|------------------|
| `ui_system_internal.h` | ~210 | Internal data structures (`KainNativeUiNode`, `KainNativeUiSession`, fixed-size arrays, hash tables) | `base.h`, `ui_system.h`, `component_surface.h` |
| `ui_system.c` | ~2,600 | **Core engine**: session lifecycle, node CRUD, style/state storage, event ring buffer, hit-testing, focus, IME, drag-drop, menus, dialogs, clipboard, resources, fonts, draw commands, hash utilities, per-frame arena | `ui_system_internal.h`, `ui_host_adapter.h`, `ui_font.h`, **`stb_truetype.h`** |
| `ui_host_adapter.h` | ~15 | Minimal host adapter interface (7 functions) | `ui_system_internal.h` |
| `ui_host_adapter.c` | ~520 | **Win32 GDI backend**: window creation, DIB framebuffer, WM_PAINT with BitBlt, message pumping, input bridging, DPI-aware (`SetProcessDpiAwarenessContext`), WM_SIZE DIB recreation, WM_DPICHANGED | `win32.h`, `ui_renderer.h`, `ui_layout.h`, `input_system.h` |
| `native_ui_surface.c` | ~280 | KainComponentSurface vtable adapter — bridges compiler to native runtime | `ui_system.h`, `component_surface.h` |
| `ui_color.c` | ~220 | Color parsing (#hex, rgba, named), alpha blending (`Z3-proven div255_fast`), opacity | `ui_color.h` |
| `ui_compiled_bundle.c` | ~610 | JSON bundle deserializer for compiler-compiled UI trees | `ui_bundle.h` |
| `ui_hot_reload.c` | ~650 | Shared-memory IPC for live UI reloading, file signature watcher, controller | `ui_hot_reload.h`, `<windows.h>`, `<sys/mman.h>` |
| `ui_layout.c` | ~220 | Flexbox-style layout engine: direction, padding, spacing, gap, recursive resolution | `ui_layout.h`, `ui_system_internal.h` |
| `ui_renderer.c` | ~350 | **Software framebuffer renderer**: clear, fill rect, border rect, rounded rect, glyph text via stb_truetype, DPI-scaled node rendering | `ui_renderer.h`, `ui_color.h`, `ui_font.h`, `ui_system_internal.h` |
| `ui_runtime.c` | ~1,000 | High-level compiled-bundle runtime: validation, focus routing, event routing, text editing, hot-reload state transfer | `ui_runtime.h`, `version.h` |

> **Note**: `ui_compiled_bundle.c`, `ui_hot_reload.c`, `ui_runtime.c` are not linked into the standalone demos — they're used by the Kain compiler's bundle system. The standalone demos use the raw `abi_ui_*` ABI.

---

## Public API (`include/ui_system.h`)

The public ABI exposes **~90 functions** across these categories:

### Session Lifecycle
```c
int64_t abi_ui_session_create(const char* app_name, int64_t width, int64_t height);
int64_t abi_ui_session_destroy(int64_t session_id);
int64_t abi_ui_window_open(int64_t session_id, const char* title, int64_t width, int64_t height);
```

### Frame Lifecycle
```c
int64_t abi_ui_begin_frame(int64_t session_id, double delta_ms);
int64_t abi_ui_end_frame(int64_t session_id);
int64_t abi_ui_host_attach(int64_t session_id, const char* backend_id);  // "winit", "vulkan", etc.
```

### Node Management
```c
int64_t abi_ui_node_create(int64_t session_id, const char* kind);
int64_t abi_ui_node_destroy(int64_t session_id, int64_t node_id);
int64_t abi_ui_node_set_parent(int64_t session_id, int64_t node_id, int64_t parent_id);
int64_t abi_ui_node_set_rect(int64_t session_id, int64_t node_id, double x, double y, double width, double height);
int64_t abi_ui_node_set_text(int64_t session_id, int64_t node_id, const char* text);
int64_t abi_ui_node_set_stable_key(int64_t session_id, int64_t node_id, const char* stable_key);
int64_t abi_ui_node_find_by_stable_key(int64_t session_id, const char* stable_key);
```

### Styles & Flags
```c
int64_t abi_ui_node_set_style_i64/f64/string(...);
int64_t abi_ui_node_set_flag(int64_t session_id, int64_t node_id, const char* flag, int64_t enabled);
// flags: "hidden", "focusable", "interactive", "disabled", "hovered", "pressed"
```

### Events
```c
int64_t abi_ui_push_event(int64_t session_id, const char* kind, ...);
int64_t abi_ui_poll_event(int64_t session_id);
int64_t abi_ui_hit_test(int64_t session_id, double x, double y);
```

### Resources & Fonts
```c
int64_t abi_ui_font_create(int64_t session_id, const char* key, const char* family, double size);
int64_t abi_ui_resource_set_bytes(int64_t session_id, int64_t resource_id, const uint8_t* bytes, int64_t byte_length);
```

### Font ABI (`include/ui_font.h`)
```c
int64_t abi_ui_font_load_ttf(int64_t session_id, const char* key, const char* family, double size, const uint8_t* ttf_data, int64_t ttf_len);
KainUiGlyph* abi_ui_font_get_glyph(int64_t session_id, int64_t font_id, int codepoint);
void abi_ui_font_release_glyph(KainUiGlyph* glyph);
int kain_ui_font_get_vmetrics(int64_t session_id, int64_t font_resource_id, int* ascent, int* descent, int* line_gap);
```

Full symbol list and signatures in `include/ui_system.h` and `include/ui_font.h`.

---

## Widget Library (`widgets/`)

An **immediate-mode-style** widget library built ON TOP of the retained-mode ABI. No GDI — all text rendered via stb_truetype glyph rasterization.

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

### Font Loading (cross-platform)
```c
// Load from explicit path:
ui_widget_load_font(ctx, "/path/to/font.ttf", 16.0);

// Search platform system fonts:
ui_widget_load_default_font(ctx, 16.0);  // Windows: segoeui.ttf, macOS: Helvetica, Linux: DejaVuSans
```

### Key Design
- `KainUiWidgetContext` tracks session, layout cursor, hover/press state, font table
- Layout helpers: `ui_layout_row(ctx, n, widths[])`, `ui_layout_column(ctx, n, heights[])`
- Font table: up to `UI_WIDGET_MAX_FONTS` (8) fonts loaded simultaneously
- **Files**: `widgets/ui_widget.h` (~250 lines), `widgets/ui_widget.c` (~1,200 lines)

---

## stb_truetype Integration

The font rasterizer lives at `extras/_stb-truetype/stb_truetype.h` (public domain, single-header).

### How it's wired:
1. **Font loading**: `abi_ui_font_load_ttf()` → `abi_ui_resource_set_bytes()` → `stbtt_InitFont()` in `ui_system.c`
2. **Glyph retrieval**: `abi_ui_font_get_glyph()` → `stbtt_GetCodepointBitmap()` with 256-entry LRU cache
3. **Text measurement**: `abi_ui_text_measure_width/height()` → `stbtt_GetCodepointHMetrics()` / `stbtt_GetFontVMetrics()`
4. **Rendering**: `ui_render_glyph_text()` in `ui_renderer.c` blends alpha masks into framebuffer (integer math, no floating point)
5. **Widget text**: `ui_widget_draw_text()` → `abi_ui_font_get_glyph()` → pixel blending

### Z3 Proofs: 21 SMT2 + 21 YAML packs at `extras/_stb-truetype/z3/`
- Bezier convex hull correctness
- Scale/pixel-height division safety  
- Glyph bitmap bounds non-overflow
- hmtx table index bounds
- scanline AA coverage clamping [0, 255]
- Winding rule accumulation
- Edge clip arithmetic
- Sort comparator total order

---

## DPI & Window Resize Handling

All modern DPI support is in `ui_host_adapter.c`:

| Feature | Implementation |
|---------|---------------|
| **Per-monitor DPI v2** | `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` loaded defensively via `GetProcAddress` (safe on Win 8.1+) |
| **Initial DPI query** | `GetDeviceCaps(dc, LOGPIXELSX) / 96.0f` stored in `host->dpi_scale` |
| **DPI threaded to renderer** | `session->dpi_scale = host->dpi_scale` |
| **Node position scaling** | `ui_render_node()` multiplies `node->x/y/width/height` by `session->dpi_scale` |
| **WM_SIZE DIB recreation** | On resize: `SelectObject(old) → DeleteObject → CreateDIBSection(new) → memset(0)` |
| **WM_DPICHANGED** | Uses Windows-provided rect → `SetWindowPos()` → auto-triggers WM_SIZE → DIB recreation |

---

## Build System

### Single Makefile (`Makefile`)

The master Makefile covers everything:

```bash
# Build everything
make                     # static lib + tests + demos
make -j8                 # parallel build

# Individual targets
make static              # build libkain_ui.a (599KB, 12 objects)
make libs                # build .o files only (incremental)
make tests               # build test_ui/ executables
make demos               # build test_ui_v2/ executables
make widgets             # build widget test

# Build + run
make run_cosmic          # cosmic dashboard
make run_retro           # retro wave
make run_3d              # 3D sandbox

# Clean
make clean
```

### Key features
- **Incremental compilation**: `.o` files cached, only changed `.c` files recompile
- **Dependency tracking**: Auto-generated `.d` files track header dependencies
- **Static library**: `libkain_ui.a` aggregates all 12 UI + core objects
- **Auto MSVC detection**: Finds Visual Studio + SDK library paths automatically
- **Usage**: `make CC=gcc` or `make CFLAGS=-O2`

### Legacy build.bat files
Each test directory also has its own `build.bat` for standalone compilation.

---

## Tests (`test_ui/`)

9 test executables demonstrating the UI system:

| Test | Lines | What It Demonstrates |
|------|-------|---------------------|
| `calculator.c` | 506 | Working 4-function calculator with click/keyboard input, styled buttons |
| `full_demo.c` | 489 | Rich dashboard with sidebar, animated cards, bar chart, input logging |
| `keypad.c` | 483 | PIN entry with masked display, access state machine |
| `anim_demo.c` | 441 | 100-particle physics simulation with bouncing, color cycling |
| `hot_reload_test.c` | 344 | Shared memory IPC channel test |
| `renderer_smoke_test.c` | ~400 | **First test to call `ui_render_frame()` with real nodes** — Oracle-verified |
| `widget_hello.c` | ~450 | Minimal widget library hello world |
| `widget_demo.c` | ~700 | Comprehensive widget demo |
| `widget_calculator.c` | ~500 | Calculator built with widget system |

---

## Demos (`test_ui_v2/`)

3 brand-new, visually groundbreaking demos pushing the renderer to its limits:

### 🪐 Cosmic Dashboard (1,763 lines)
```
350 parallax particles · nebula sine-wave gradients · 6 glass-morphism panels
10 fonts · live waveform · rotating particle flux ring · command console
stellar constellation chart · orbiting compass · CPU/MEM gauges
```
- `cosmic_dashboard.exe` (552KB)
- **Oracle**: Window found, content rendering

### 🌴 Retro Wave 2084 (1,540 lines)
```
Scrolling perspective grid · neon sunset with scanlines · 3-axis wireframe cube
5 transparent glowing panels · 8-bar equalizer · Matrix rain · bouncing cassettes
3 color schemes · multi-pass glow · glitch effect · 6 fonts
```
- `retrowave.exe` (881KB)
- **Oracle**: 85% pixel animation rate 🔥

### 🧊 3D UI Sandbox (1,330 lines)
```
3D rotation matrices · perspective/orthographic projection · painter's algorithm
isometric grid floor · Z-depth floating panels · particle fountain with gravity
120-star parallax starfield · exploding cube animation · DIB re-creation on resize
```
- `ui3d_sandbox.exe` (881KB)
- **Oracle**: Window found, interactive

---

## Z3 Proof Coverage

### Renderer Correctness (4 SMT2 + 4 YAML at `z3/proofs/`)
| Proof | What It Proves |
|-------|---------------|
| `ui-renderer-sibling-bounds-safe` | `child_idx` always in valid `[0, MAX_NODES-1]` range |
| `ui-renderer-children-always-traversed` | Children rendered regardless of parent size |
| `ui-renderer-fb-clear-no-aliasing` | `memcpy` framebuffer clear is strict-aliasing safe |
| `ui-layout-no-stack-overflow` | Stack allocation bounded < 2KB |

### All Renderer Proofs (18 SMT2 + 18 YAML at `z3/proofs-experimental/`)
Branchless clamp, alpha blend, flag batch test, SIMD fill, sibling-linked list speedup, dirty flag caching, stable key collision, incremental index update, mask bounds, event count bounded, draw command count bounded, heap-free arena, tagged pointers

### Font Proofs (21 SMT2 + 21 YAML at `extras/_stb-truetype/z3/proofs/`)
Bezier math, bounds, scale, sort edges, glyph metrics, find table, IsGlyphEmpty, clip line, active edges, subpixel box, AA coverage, scanline AA, coverage invariant, sort stability

---

## External Dependencies

| Library | Used For | Header/Link |
|---------|----------|-------------|
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
| `RENDERER_BACKEND` | — | `component_surface.c` | Selects GPU backend (vulkan/d3d12/webgpu) |
| `LIB` | auto-detected | Makefile | MSVC library search path |

---

## How to Write a New UI Demo

### Minimal skeleton (raw ABI, no widgets):

```c
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "ui_system.h"
#include "ui_system_internal.h"  // for KainNativeUiSession, KainWin32UiHost
#include "ui_host_adapter.h"
#include "ui_renderer.h"
#include "ui_layout.h"
#include "ui_color.h"

// Stub: char* string_new(char* src);
//       double kain_clampd(double value, double min, double max);

int main(void) {
    // 1. Create session + window
    int64_t sid = abi_ui_session_create("MyDemo", 1280, 720);
    abi_ui_window_open(sid, "My Demo", 1280, 720);
    abi_ui_host_attach(sid, "winit");

    // 2. Subclass window for WM_PAINT (fixes host adapter BitBlt bug)
    KainNativeUiSession* s = (KainNativeUiSession*)abi_ui_find_session(sid);
    KainWin32UiHost* host = (KainWin32UiHost*)s->host_state;
    SetWindowLongPtrA(host->hwnd, GWLP_USERDATA, (LONG_PTR)host);
    SetWindowLongPtrA(host->hwnd, GWLP_WNDPROC, (LONG_PTR)my_wndproc);

    // 3. Frame loop (manual PeekMessage, NOT abi_ui_host_pump)
    while (running) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { ... }

        abi_ui_begin_frame(sid, dt);
        // Write to host->framebuffer[...] directly
        abi_ui_end_frame(sid);

        InvalidateRect(host->hwnd, NULL, FALSE);  // triggers WM_PAINT
        Sleep(16);
    }
}
```

### With widgets + fonts:

```c
#include "ui_widget.h"
#include "ui_font.h"

KainUiWidgetContext* ctx = ui_widget_create(sid);
ui_widget_load_font(ctx, "C:/Windows/Fonts/arial.ttf", 16.0);

while (running) {
    ui_widget_begin_frame(ctx);
    if (ui_button(ctx, "Click me!")) { /* clicked */ }
    ui_widget_end_frame(ctx);

    InvalidateRect(host->hwnd, NULL, FALSE);
}
```

---

## Known Limitations

1. **Single-threaded**: All rendering and event processing is single-threaded
2. **Win32-only host**: The "winit" backend is Windows GDI only. Linux/macOS host adapters are future work
3. **No hardware acceleration**: Software framebuffer only. GPU backends (Vulkan/D3D12/WebGPU) are cataloged but not implemented
4. **Font hinting**: stb_truetype basic hinting is used but no ClearType or subpixel rendering
5. **No widget theming engine**: Colors and sizes are hardcoded per widget

---

*Generated from source analysis. Total UI subsystem: ~7,000 lines of C across 12 source files in `runtime/native/src/ui/` plus headers in `runtime/native/include/`.*
